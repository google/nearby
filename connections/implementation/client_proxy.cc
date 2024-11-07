// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "connections/implementation/client_proxy.h"

#include <cstdint>
#include <functional>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/connections_device_provider.h"
#include "connections/v3/listeners.h"
#include "internal/analytics/event_logger.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/os_name.h"
#include "internal/platform/prng.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {
using ::location::nearby::connections::OsInfo;

constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

bool IsFeatureUseStableEndpointIdEnabled() {
  return NearbyFlags::GetInstance().GetBoolFlag(
      connections::config_package_nearby::nearby_connections_feature::
          kUseStableEndpointId);
}

}  // namespace

// The definition is necessary before C++17.
constexpr absl::Duration
    ClientProxy::kHighPowerAdvertisementEndpointIdCacheTimeout;

ClientProxy::ClientProxy(::nearby::analytics::EventLogger* event_logger)
    : client_id_(Prng().NextInt64()) {
  NEARBY_LOGS(INFO) << "ClientProxy ctor event_logger=" << event_logger;
  analytics_recorder_ =
      std::make_unique<analytics::AnalyticsRecorder>(event_logger);
  error_code_recorder_ = std::make_unique<ErrorCodeRecorder>(
      [this](const ErrorCodeParams& params) {
        analytics_recorder_->OnErrorCode(params);
      });
  local_os_info_.set_type(
      OSNameToOsInfoType(api::ImplementationPlatform::GetCurrentOS()));
  supports_safe_to_disconnect_ = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::
          kEnableSafeToDisconnect);
  support_auto_reconnect_ = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::kEnableAutoReconnect);
  local_safe_to_disconnect_version_ = NearbyFlags::GetInstance().GetInt64Flag(
      config_package_nearby::nearby_connections_feature::
          kSafeToDisconnectVersion);
  NEARBY_LOGS(INFO) << "[safe-to-disconnect]: Local enabled: "
                    << supports_safe_to_disconnect_
                    << "; Version: " << local_safe_to_disconnect_version_;
}

ClientProxy::~ClientProxy() { Reset(); }

std::int64_t ClientProxy::GetClientId() const { return client_id_; }

std::string ClientProxy::GetLocalEndpointId() {
  MutexLock lock(&mutex_);
  if (!local_endpoint_id_.empty()) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Reusing cached endpoint id: " << local_endpoint_id_;
    return local_endpoint_id_;
  }
  if (external_device_provider_ == nullptr) {
    local_endpoint_id_ = GenerateLocalEndpointId();
    NEARBY_LOGS(INFO) << __func__ << ": Locally generating endpoint id: "
                      << local_endpoint_id_;
  } else {
    local_endpoint_id_ =
        external_device_provider_->GetLocalDevice()->GetEndpointId();
    NEARBY_LOGS(INFO)
        << __func__
        << ": From external device provider, populating endpoint id: "
        << local_endpoint_id_;
  }
  return local_endpoint_id_;
}

const NearbyDevice* ClientProxy::GetLocalDevice() {
  if (external_device_provider_ == nullptr &&
      connections_device_provider_ == nullptr) {
    // TODO(b/285602283): Plug in actual endpoint info once available.
    auto provider =
        v3::ConnectionsDeviceProvider(GetLocalEndpointId(), "V3 endpoint", {});
    RegisterConnectionsDeviceProvider(
        std::make_unique<v3::ConnectionsDeviceProvider>(provider));
  }
  return GetLocalDeviceProvider()->GetLocalDevice();
}

std::string ClientProxy::GetConnectionToken(const std::string& endpoint_id) {
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_token;
  }
  return {};
}

std::optional<std::string> ClientProxy::GetBluetoothMacAddress(
    const std::string& endpoint_id) {
  auto item = bluetooth_mac_addresses_.find(endpoint_id);
  if (item != bluetooth_mac_addresses_.end()) return item->second;
  return std::nullopt;
}

void ClientProxy::SetBluetoothMacAddress(
    const std::string& endpoint_id, const std::string& bluetooth_mac_address) {
  bluetooth_mac_addresses_[endpoint_id] = bluetooth_mac_address;
}

std::string ClientProxy::GenerateLocalEndpointId() {
  if (IsFeatureUseStableEndpointIdEnabled()) {
    if (!cached_endpoint_id_.empty()) {
      if (stable_endpoint_id_mode_) {
        NEARBY_LOGS(INFO) << "ClientProxy [Local Endpoint Re-using cached "
                             "endpoint id due to in stable endpoint id mode]: "
                             "client="
                          << GetClientId()
                          << "; cached_endpoint_id_=" << cached_endpoint_id_;
        return cached_endpoint_id_;
      }
    }
  } else {
    if (high_vis_mode_) {
      if (!cached_endpoint_id_.empty()) {
        NEARBY_LOGS(INFO) << "ClientProxy [Local Endpoint Re-using cached "
                             "endpoint id]: client="
                          << GetClientId()
                          << "; cached_endpoint_id_=" << cached_endpoint_id_;
        return cached_endpoint_id_;
      }
    }
  }
  std::string id;
  Prng prng;
  for (int i = 0; i < kEndpointIdLength; i++) {
    id += kEndpointIdChars[prng.NextUint32() % sizeof(kEndpointIdChars)];
  }
  return id;
}

void ClientProxy::Reset() {
  MutexLock lock(&mutex_);

  StoppedAdvertising();
  StoppedDiscovery();
  RemoveAllEndpoints();
  if (IsFeatureUseStableEndpointIdEnabled()) {
    ExitStableEndpointIdMode();
  } else {
    ExitHighVisibilityMode();
  }
}

void ClientProxy::StartedAdvertising(
    const std::string& service_id, Strategy strategy,
    const ConnectionListener& listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const AdvertisingOptions& advertising_options) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [StartedAdvertising]: client="
                    << GetClientId();

  if (IsFeatureUseStableEndpointIdEnabled()) {
    if (stable_endpoint_id_mode_) {
      cached_endpoint_id_ = local_endpoint_id_;
    } else {
      cached_endpoint_id_.clear();
    }

    CancelClearCachedEndpointIdAlarm();
  } else {
    if (high_vis_mode_) {
      cached_endpoint_id_ = local_endpoint_id_;
      NEARBY_LOGS(INFO)
          << "ClientProxy [High Visibility Mode Adv, Cache EndpointId]: client="
          << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
      CancelClearCachedEndpointIdAlarm();
    }
  }

  advertising_info_ = {service_id, listener};
  advertising_options_ = advertising_options;

  const std::vector<location::nearby::proto::connections::Medium> medium_vector(
      mediums.begin(), mediums.end());
  analytics_recorder_->OnStartAdvertising(strategy, medium_vector, false, 0);
}

void ClientProxy::StoppedAdvertising() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [StoppedAdvertising]: client="
                    << GetClientId();

  if (IsAdvertising()) {
    advertising_info_.Clear();
    analytics_recorder_->OnStopAdvertising();
    local_endpoint_info_.clear();
  }
  // advertising_options_ is purposefully not cleared here.
  OnSessionComplete();

  if (IsFeatureUseStableEndpointIdEnabled()) {
    ExitStableEndpointIdMode();
  } else {
    ExitHighVisibilityMode();
  }
}

bool ClientProxy::IsAdvertising() const {
  MutexLock lock(&mutex_);

  return !advertising_info_.IsEmpty();
}

std::string ClientProxy::GetAdvertisingServiceId() const {
  MutexLock lock(&mutex_);
  return advertising_info_.service_id;
}

void ClientProxy::StartedListeningForIncomingConnections(
    absl::string_view service_id, Strategy strategy,
    v3::ConnectionListener listener,
    const v3::ConnectionListeningOptions& options) {
  MutexLock lock(&mutex_);
  listening_options_ = options;
  listening_info_ = ListeningInfo{
      .service_id = std::string(service_id),
      .listener = std::move(listener),
  };
  analytics_recorder_->OnStartedIncomingConnectionListening(strategy);
}

void ClientProxy::StoppedListeningForIncomingConnections() {
  MutexLock lock(&mutex_);
  listening_info_.Clear();
  analytics_recorder_->OnStoppedIncomingConnectionListening();
}

bool ClientProxy::IsListeningForIncomingConnections() const {
  MutexLock lock(&mutex_);
  return !listening_info_.IsEmpty();
}

std::string ClientProxy::GetListeningForIncomingConnectionsServiceId() const {
  MutexLock lock(&mutex_);
  if (IsListeningForIncomingConnections()) {
    return listening_info_.service_id;
  }
  return "";
}

ConnectionListener ClientProxy::GetAdvertisingOrIncomingConnectionListener() {
  if (IsListeningForIncomingConnections()) {
    ConnectionListener listener = {
        .initiated_cb =
            [this](const std::string& endpoint_id,
                   const ConnectionResponseInfo& info) {
              auto remote_device = v3::ConnectionsDevice(
                  endpoint_id, info.remote_endpoint_info.AsStringView(), {});
              this->listening_info_.listener.initiated_cb(
                  remote_device,
                  v3::InitialConnectionInfo{
                      .authentication_digits = info.authentication_token,
                      .raw_authentication_token =
                          info.raw_authentication_token.string_data(),
                      .is_incoming_connection = info.is_incoming_connection,
                  });
            },
        .accepted_cb =
            [this](const std::string& endpoint_id) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              this->listening_info_.listener.result_cb(
                  remote_device,
                  v3::ConnectionResult{.status = Status{
                                           .value = Status::kSuccess,
                                       }});
            },
        .rejected_cb =
            [this](const std::string& endpoint_id, Status status) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              this->listening_info_.listener.result_cb(
                  remote_device, v3::ConnectionResult{.status = status});
            },
        .disconnected_cb =
            [this](const std::string& endpoint_id) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              this->listening_info_.listener.disconnected_cb(remote_device);
            },
        .bandwidth_changed_cb =
            [this](const std::string& endpoint_id, Medium medium) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              this->listening_info_.listener.bandwidth_changed_cb(
                  remote_device, v3::BandwidthInfo{.medium = medium});
            },
    };
    return listener;
  }
  return advertising_info_.listener;
}

void ClientProxy::StartedDiscovery(
    const std::string& service_id, Strategy strategy,
    DiscoveryListener listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const DiscoveryOptions& discovery_options) {
  MutexLock lock(&mutex_);
  discovery_info_ = DiscoveryInfo{service_id, std::move(listener)};
  discovery_options_ = discovery_options;

  const std::vector<location::nearby::proto::connections::Medium> medium_vector(
      mediums.begin(), mediums.end());
  analytics_recorder_->OnStartDiscovery(strategy, medium_vector, false, 0);
}

void ClientProxy::StoppedDiscovery() {
  MutexLock lock(&mutex_);

  if (IsDiscovering()) {
    discovered_endpoint_ids_.clear();
    discovery_info_.Clear();
    analytics_recorder_->OnStopDiscovery();
  }
  // discovery_options_ is purposefully not cleared here.
  OnSessionComplete();
}

bool ClientProxy::IsDiscoveringServiceId(const std::string& service_id) const {
  MutexLock lock(&mutex_);

  return IsDiscovering() && service_id == discovery_info_.service_id;
}

bool ClientProxy::IsDiscovering() const {
  MutexLock lock(&mutex_);

  return !discovery_info_.IsEmpty();
}

std::string ClientProxy::GetDiscoveryServiceId() const {
  MutexLock lock(&mutex_);

  return discovery_info_.service_id;
}

void ClientProxy::OnEndpointFound(
    const std::string& service_id, const std::string& endpoint_id,
    const ByteArray& endpoint_info,
    location::nearby::proto::connections::Medium medium) {
  MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "ClientProxy [Endpoint Found]: [enter] id="
                    << endpoint_id << "; service=" << service_id
                    << "; info=" << absl::BytesToHexString(endpoint_info.data())
                    << "; medium="
                    << location::nearby::proto::connections::Medium_Name(
                           medium);
  if (!IsDiscoveringServiceId(service_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Endpoint Found]: Ignoring event for id="
                      << endpoint_id
                      << " because this client is not discovering.";
    return;
  }

  if (discovered_endpoint_ids_.count(endpoint_id)) {
    NEARBY_LOGS(WARNING)
        << "ClientProxy [Endpoint Found]: Ignoring event for id=" << endpoint_id
        << " because this client has already reported this endpoint as found.";
    return;
  }

  discovered_endpoint_ids_.insert(endpoint_id);
  discovery_info_.listener.endpoint_found_cb(endpoint_id, endpoint_info,
                                             service_id);
  analytics_recorder_->OnEndpointFound(medium);
}

void ClientProxy::OnEndpointLost(const std::string& service_id,
                                 const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "ClientProxy [Endpoint Lost]: [enter] id=" << endpoint_id
                    << "; service=" << service_id;
  if (!IsDiscoveringServiceId(service_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Endpoint Lost]: Ignoring event for id="
                      << endpoint_id
                      << " because this client is not discovering.";
    return;
  }

  const auto it = discovered_endpoint_ids_.find(endpoint_id);
  if (it == discovered_endpoint_ids_.end()) {
    NEARBY_LOGS(WARNING)
        << "ClientProxy [Endpoint Lost]: Ignoring event for id=" << endpoint_id
        << " because this client has not yet reported this endpoint as found";
    return;
  }

  discovered_endpoint_ids_.erase(it);
  discovery_info_.listener.endpoint_lost_cb(endpoint_id);
}

void ClientProxy::OnRequestConnection(
    const Strategy& strategy, const std::string& endpoint_id,
    const ConnectionOptions& connection_options) {
  NEARBY_LOGS(INFO) << "ClientProxy [RequestConnection]: id=" << endpoint_id;
  analytics_recorder_->OnRequestConnection(strategy, endpoint_id);
}

void ClientProxy::OnConnectionInitiated(
    const std::string& endpoint_id, const ConnectionResponseInfo& info,
    const ConnectionOptions& connection_options,
    const ConnectionListener& listener, const std::string& connection_token) {
  MutexLock lock(&mutex_);

  // Whether this is incoming or outgoing, the local and remote endpoints both
  // still need to accept this connection, so set its establishment status to
  // PENDING.
  auto result = connections_.emplace(
      endpoint_id, std::make_pair(
                       Connection{
                           .is_incoming = info.is_incoming_connection,
                           .connection_listener = listener,
                           .connection_options = connection_options,
                           .connection_token = connection_token,
                       },
                       PayloadListener{
                           .payload_cb = [](absl::string_view, Payload) {},
                           .payload_progress_cb = [](absl::string_view,
                                                     PayloadProgressInfo) {},
                       }));
  // Instead of using structured binding which is nice, but banned
  // (can not use c++17 features, until chromium does) we unpack manually.
  auto& pair_iter = result.first;
  bool inserted = result.second;
  NEARBY_LOGS(INFO)
      << "ClientProxy [Connection Initiated]: add Connection: client="
      << GetClientId() << "; endpoint_id=" << endpoint_id
      << "; inserted=" << inserted;
  DCHECK(inserted);
  const ConnectionPair& item = pair_iter->second;
  // Notify the client.
  //
  // Note: we allow devices to connect to an advertiser even after it stops
  // advertising, so no need to check IsAdvertising() here.
  item.first.connection_listener.initiated_cb(endpoint_id, info);

  if (info.is_incoming_connection) {
    // Add CancellationFlag for advertisers once encryption succeeds.
    AddCancellationFlag(endpoint_id);
    analytics_recorder_->OnConnectionRequestReceived(endpoint_id);
  } else {
    analytics_recorder_->OnConnectionRequestSent(endpoint_id);
  }
}

void ClientProxy::OnConnectionAccepted(const std::string& endpoint_id) {
  NEARBY_LOGS(INFO) << "ClientProxy [ConnectionAccepted]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Connection Accepted]: no pending "
                         "connection; endpoint_id="
                      << endpoint_id;
    return;
  }

  // Notify the client.
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.connection_listener.accepted_cb(endpoint_id);
    item->first.status = Connection::kConnected;
  }
}

void ClientProxy::OnConnectionRejected(const std::string& endpoint_id,
                                       const Status& status) {
  NEARBY_LOGS(INFO) << "ClientProxy [ConnectionRejected]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Connection Rejected]: no pending "
                         "connection; endpoint_id="
                      << endpoint_id;
    return;
  }

  // Notify the client.
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.connection_listener.rejected_cb(endpoint_id, status);
    OnDisconnected(endpoint_id, false /* notify */);
  }
}

void ClientProxy::OnBandwidthChanged(const std::string& endpoint_id,
                                     Medium new_medium) {
  NEARBY_LOGS(INFO) << "ClientProxy [BandwidthChanged]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.connection_listener.bandwidth_changed_cb(endpoint_id,
                                                         new_medium);
    NEARBY_LOGS(INFO) << "ClientProxy [reporting onBandwidthChanged]: client="
                      << GetClientId() << "; endpoint_id=" << endpoint_id;
  }
}

void ClientProxy::OnDisconnected(const std::string& endpoint_id, bool notify) {
  NEARBY_LOGS(INFO) << "ClientProxy [OnDisconnected]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    if (notify) {
      item->first.connection_listener.disconnected_cb({endpoint_id});
    }
    connections_.erase(endpoint_id);
    OnSessionComplete();
  }

  CancelEndpoint(endpoint_id);

  if (IsFeatureUseStableEndpointIdEnabled()) {
    if (!stable_endpoint_id_mode_ && !HasOngoingConnection()) {
      ScheduleClearCachedEndpointIdAlarm();
    }
  }
}

bool ClientProxy::ConnectionStatusMatches(const std::string& endpoint_id,
                                          Connection::Status status) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.status == status;
  }
  return false;
}

BooleanMediumSelector ClientProxy::GetUpgradeMediums(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.allowed;
  }
  return {};
}

bool ClientProxy::Is5GHzSupported(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.connection_info.supports_5_ghz;
  }
  return false;
}

std::string ClientProxy::GetBssid(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.connection_info.bssid;
  }
  return {};
}

std::int32_t ClientProxy::GetApFrequency(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.connection_info.ap_frequency;
  }
  return -1;
}

std::string ClientProxy::GetIPAddress(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.connection_info.ip_address;
  }
  return {};
}

bool ClientProxy::IsConnectedToEndpoint(const std::string& endpoint_id) const {
  return ConnectionStatusMatches(endpoint_id, Connection::kConnected);
}

std::vector<std::string> ClientProxy::GetMatchingEndpoints(
    absl::AnyInvocable<bool(const Connection&)> pred) const {
  MutexLock lock(&mutex_);

  std::vector<std::string> connected_endpoints;

  for (const auto& pair : connections_) {
    const auto& endpoint_id = pair.first;
    const auto& connection_pair = pair.second;
    if (pred(connection_pair.first)) {
      connected_endpoints.push_back(endpoint_id);
    }
  }
  return connected_endpoints;
}

std::vector<std::string> ClientProxy::GetPendingConnectedEndpoints() const {
  return GetMatchingEndpoints([](const Connection& connection) {
    return connection.status != Connection::kConnected;
  });
}

std::vector<std::string> ClientProxy::GetConnectedEndpoints() const {
  return GetMatchingEndpoints([](const Connection& connection) {
    return connection.status == Connection::kConnected;
  });
}

bool ClientProxy::HasOngoingConnection() const {
  return !GetPendingConnectedEndpoints().empty() ||
         !GetConnectedEndpoints().empty();
}

std::int32_t ClientProxy::GetNumOutgoingConnections() const {
  return GetMatchingEndpoints([](const Connection& connection) {
           return connection.status == Connection::kConnected &&
                  !connection.is_incoming;
         })
      .size();
}

std::int32_t ClientProxy::GetNumIncomingConnections() const {
  return GetMatchingEndpoints([](const Connection& connection) {
           return connection.status == Connection::kConnected &&
                  connection.is_incoming;
         })
      .size();
}

bool ClientProxy::IsIncomingConnection(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr && item->first.status == Connection::kConnected) {
    return item->first.is_incoming;
  }
  return false;
}

bool ClientProxy::IsOutgoingConnection(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr && item->first.status == Connection::kConnected) {
    return !item->first.is_incoming;
  }
  return false;
}

bool ClientProxy::HasPendingConnectionToEndpoint(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.status != Connection::kConnected;
  }
  return false;
}

bool ClientProxy::HasLocalEndpointResponded(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kLocalEndpointAccepted |
                                      Connection::kLocalEndpointRejected));
}

bool ClientProxy::HasRemoteEndpointResponded(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kRemoteEndpointAccepted |
                                      Connection::kRemoteEndpointRejected));
}

void ClientProxy::LocalEndpointAcceptedConnection(
    const std::string& endpoint_id, PayloadListener listener) {
  MutexLock lock(&mutex_);
  if (HasLocalEndpointResponded(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "ClientProxy [Local Accepted]: local endpoint has responded; id="
        << endpoint_id;
    return;
  }
  AppendConnectionStatus(endpoint_id, Connection::kLocalEndpointAccepted);
  NEARBY_LOGS(INFO) << "ClientProxy [Local Accepted]: id=" << endpoint_id;
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->second = std::move(listener);
  }
  analytics_recorder_->OnLocalEndpointAccepted(endpoint_id);
}

void ClientProxy::LocalEndpointRejectedConnection(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  if (HasLocalEndpointResponded(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "ClientProxy [Local Rejected]: local endpoint has responded; id="
        << endpoint_id;
    return;
  }

  AppendConnectionStatus(endpoint_id, Connection::kLocalEndpointRejected);
  analytics_recorder_->OnLocalEndpointRejected(endpoint_id);
}

void ClientProxy::RemoteEndpointAcceptedConnection(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  if (HasRemoteEndpointResponded(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "ClientProxy [Remote Accepted]: remote endpoint has responded; id="
        << endpoint_id;
    return;
  }

  AppendConnectionStatus(endpoint_id, Connection::kRemoteEndpointAccepted);
  analytics_recorder_->OnRemoteEndpointAccepted(endpoint_id);
}

void ClientProxy::RemoteEndpointRejectedConnection(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  if (HasRemoteEndpointResponded(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "ClientProxy [Remote Rejected]: remote endpoint has responded; id="
        << endpoint_id;
    return;
  }

  AppendConnectionStatus(endpoint_id, Connection::kRemoteEndpointRejected);
  analytics_recorder_->OnRemoteEndpointRejected(endpoint_id);
}

bool ClientProxy::IsConnectionAccepted(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  return ConnectionStatusesContains(endpoint_id,
                                    Connection::kLocalEndpointAccepted) &&
         ConnectionStatusesContains(endpoint_id,
                                    Connection::kRemoteEndpointAccepted);
}

bool ClientProxy::IsConnectionRejected(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kLocalEndpointRejected |
                                      Connection::kRemoteEndpointRejected));
}

bool ClientProxy::LocalConnectionIsAccepted(std::string endpoint_id) const {
  return ConnectionStatusesContains(
      endpoint_id, ClientProxy::Connection::kLocalEndpointAccepted);
}

bool ClientProxy::RemoteConnectionIsAccepted(std::string endpoint_id) const {
  return ConnectionStatusesContains(
      endpoint_id, ClientProxy::Connection::kRemoteEndpointAccepted);
}

bool ClientProxy::AutoUpgradeBandwidth() const {
  bool result = false;
  if (IsAdvertising() && (GetAdvertisingOptions().strategy.IsNone() ||
                          GetAdvertisingOptions().auto_upgrade_bandwidth)) {
    result |= true;
  }
  if (IsListeningForIncomingConnections() &&
      (GetListeningOptions().strategy.IsNone() ||
       GetListeningOptions().auto_upgrade_bandwidth)) {
    result |= true;
  }
  return result;
}

bool ClientProxy::ShouldEnforceTopologyConstraints() const {
  bool result = false;
  if (IsAdvertising() &&
      (GetAdvertisingOptions().strategy.IsNone() ||
       GetAdvertisingOptions().enforce_topology_constraints)) {
    result |= true;
  }
  if (IsListeningForIncomingConnections() &&
      (GetListeningOptions().strategy.IsNone() ||
       GetListeningOptions().enforce_topology_constraints)) {
    result |= true;
  }
  return result;
}

void ClientProxy::AddCancellationFlag(const std::string& endpoint_id) {
  // Don't insert the CancellationFlag to the map if feature flag is disabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  auto item = cancellation_flags_.find(endpoint_id);
  if (item != cancellation_flags_.end()) {
    // A new flag may be added to the map with the same endpoint, even if a
    // flag already in the map has already been cancelled, when an endpoint
    // is being reused (for example, the case when users use NS to share/receive
    // a file, then cancel in the middle because the wrong file was selected
    // and  then re-do right after). The flag needs to be uncancelled in order
    // to support a new attempt with the same endpoint.
    if (item->second->Cancelled()) {
      item->second->Uncancel();
    }
    return;
  }
  cancellation_flags_.emplace(endpoint_id,
                              std::make_unique<CancellationFlag>());
}

CancellationFlag* ClientProxy::GetCancellationFlag(
    const std::string& endpoint_id) {
  const auto item = cancellation_flags_.find(endpoint_id);
  if (item == cancellation_flags_.end()) {
    return default_cancellation_flag_.get();
  }
  return item->second.get();
}

void ClientProxy::CancelEndpoint(const std::string& endpoint_id) {
  const auto item = cancellation_flags_.find(endpoint_id);
  if (item != cancellation_flags_.end()) {
    item->second->Cancel();
  }
}

const OsInfo& ClientProxy::GetLocalOsInfo() const { return local_os_info_; }

std::optional<OsInfo> ClientProxy::GetRemoteOsInfo(
    absl::string_view endpoint_id) const {
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.os_info;
  }
  return std::nullopt;
}

void ClientProxy::SetRemoteOsInfo(absl::string_view endpoint_id,
                                  const OsInfo& remote_os_info) {
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.os_info.emplace(remote_os_info);
  }
}

std::optional<std::int32_t> ClientProxy::GetRemoteSafeToDisconnectVersion(
    absl::string_view endpoint_id) const {
  MutexLock lock(&mutex_);
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.safe_to_disconnect_version;
  }
  return std::nullopt;
}

void ClientProxy::SetRemoteSafeToDisconnectVersion(
    absl::string_view endpoint_id,
    const std::int32_t& safe_to_disconnect_version) {
  MutexLock lock(&mutex_);
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.safe_to_disconnect_version = safe_to_disconnect_version;
  }
}

bool ClientProxy::IsSafeToDisconnectEnabled(absl::string_view endpoint_id) {
  return IsSupportSafeToDisconnect() &&
         GetRemoteSafeToDisconnectVersion(endpoint_id).has_value() &&
         (GetRemoteSafeToDisconnectVersion(endpoint_id) >=
          FeatureFlags::GetInstance()
              .GetFlags()
              .min_nc_version_supports_safe_to_disconnect);
}

bool ClientProxy::IsAutoReconnectEnabled(absl::string_view endpoint_id) {
  return IsSupportAutoReconnect() &&
         GetRemoteSafeToDisconnectVersion(endpoint_id).has_value() &&
         (GetRemoteSafeToDisconnectVersion(endpoint_id) >=
          FeatureFlags::GetInstance()
              .GetFlags()
              .min_nc_version_supports_auto_reconnect);
}

bool ClientProxy::IsPayloadReceivedAckEnabled(absl::string_view endpoint_id) {
  return IsSupportSafeToDisconnect() &&
         GetRemoteSafeToDisconnectVersion(endpoint_id).has_value() &&
         (GetRemoteSafeToDisconnectVersion(endpoint_id) >=
          FeatureFlags::GetInstance()
              .GetFlags()
              .min_nc_version_supports_payload_received_ack);
}

void ClientProxy::CancelAllEndpoints() {
  for (const auto& item : cancellation_flags_) {
    CancellationFlag* cancellation_flag = item.second.get();
    if (cancellation_flag->Cancelled()) {
      continue;
    }
    cancellation_flag->Cancel();
  }
}

void ClientProxy::OnPayload(const std::string& endpoint_id, Payload payload) {
  MutexLock lock(&mutex_);

  if (IsConnectedToEndpoint(endpoint_id)) {
    const std::pair<ClientProxy::Connection, PayloadListener>* item =
        LookupConnection(endpoint_id);
    if (item != nullptr) {
      NEARBY_LOGS(INFO) << "ClientProxy [reporting onPayloadReceived]: client="
                        << GetClientId() << "; endpoint_id=" << endpoint_id
                        << " ; payload_id=" << payload.GetId();
      item->second.payload_cb(endpoint_id, std::move(payload));
    }
  }
}

const ClientProxy::ConnectionPair* ClientProxy::LookupConnection(
    absl::string_view endpoint_id) const {
  auto item = connections_.find(endpoint_id);
  return item != connections_.end() ? &item->second : nullptr;
}

ClientProxy::ConnectionPair* ClientProxy::LookupConnection(
    absl::string_view endpoint_id) {
  auto item = connections_.find(endpoint_id);
  return item != connections_.end() ? &item->second : nullptr;
}

void ClientProxy::OnPayloadProgress(const std::string& endpoint_id,
                                    const PayloadProgressInfo& info) {
  MutexLock lock(&mutex_);

  if (IsConnectedToEndpoint(endpoint_id)) {
    std::pair<ClientProxy::Connection, PayloadListener>* item =
        LookupConnection(endpoint_id);
    if (item != nullptr) {
      item->second.payload_progress_cb(endpoint_id, info);

      if (info.status == PayloadProgressInfo::Status::kInProgress) {
        NEARBY_VLOG(1) << "ClientProxy [reporting onPayloadProgress]: client="
                       << GetClientId() << "; endpoint_id=" << endpoint_id
                       << "; payload_id=" << info.payload_id
                       << ", payload_status=" << ToString(info.status);
      } else {
        NEARBY_LOGS(INFO)
            << "ClientProxy [reporting onPayloadProgress]: client="
            << GetClientId() << "; endpoint_id=" << endpoint_id
            << "; payload_id=" << info.payload_id
            << ", payload_status=" << ToString(info.status);
      }
    }
  }
}

void ClientProxy::RemoveAllEndpoints() {
  MutexLock lock(&mutex_);

  // Note: we may want to notify the client of onDisconnected() for each
  // endpoint, in the case when this is called from stopAllEndpoints(). For now,
  // just remove without notifying.
  connections_.clear();
  cancellation_flags_.clear();
  bluetooth_mac_addresses_.clear();

  OnSessionComplete();
}

void ClientProxy::OnSessionComplete() {
  MutexLock lock(&mutex_);
  if (connections_.empty() && !IsAdvertising()) {
    local_endpoint_id_.clear();

    analytics_recorder_->LogSession();
    analytics_recorder_->LogStartSession();
  }
}

bool ClientProxy::ConnectionStatusesContains(
    const std::string& endpoint_id, Connection::Status status_to_match) const {
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return (item->first.status & status_to_match) != 0;
  }
  return false;
}

void ClientProxy::AppendConnectionStatus(const std::string& endpoint_id,
                                         Connection::Status status_to_append) {
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.status =
        static_cast<Connection::Status>(item->first.status | status_to_append);
  }
}

AdvertisingOptions ClientProxy::GetAdvertisingOptions() const {
  return advertising_options_;
}

DiscoveryOptions ClientProxy::GetDiscoveryOptions() const {
  return discovery_options_;
}

v3::ConnectionListeningOptions ClientProxy::GetListeningOptions() const {
  return listening_options_;
}

void ClientProxy::EnterHighVisibilityMode() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [EnterHighVisibilityMode]: client="
                    << GetClientId();

  high_vis_mode_ = true;
}

void ClientProxy::ExitHighVisibilityMode() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [ExitHighVisibilityMode]: client="
                    << GetClientId();

  high_vis_mode_ = false;
  ScheduleClearCachedEndpointIdAlarm();
}

void ClientProxy::EnterStableEndpointIdMode() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [EnterStableEndpointIdMode]: client="
                    << GetClientId();

  stable_endpoint_id_mode_ = true;
}

void ClientProxy::ExitStableEndpointIdMode() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [ExitStableEndpointIdMode]: client="
                    << GetClientId();

  stable_endpoint_id_mode_ = false;
  ScheduleClearCachedEndpointIdAlarm();
}

void ClientProxy::ScheduleClearCachedEndpointIdAlarm() {
  CancelClearCachedEndpointIdAlarm();

  if (cached_endpoint_id_.empty()) {
    NEARBY_VLOG(1) << "ClientProxy [There is no cached local high power "
                      "advertising endpoint Id]: client="
                   << GetClientId();
    return;
  }

  if (IsFeatureUseStableEndpointIdEnabled() && HasOngoingConnection()) {
    NEARBY_VLOG(1) << "ClientProxy [Handle clearing cached endpoint ID "
                      "during disconnection]: client="
                   << GetClientId();
    return;
  }

  // Schedule to clear cache high visibility mode advertisement endpoint id in
  // 30s.
  NEARBY_LOGS(INFO) << "ClientProxy [High Visibility Mode Adv, Schedule to "
                       "Clear Cache EndpointId]: client="
                    << GetClientId()
                    << "; cached_endpoint_id_=" << cached_endpoint_id_;
  cached_endpoint_id_alarm_ =
      std::make_unique<CancelableAlarm>(
          "clear_high_power_endpoint_id_cache",
          [this]() {
            MutexLock lock(&mutex_);
            NEARBY_LOGS(INFO)
                << "ClientProxy [Cleared cached local high power advertising "
                   "endpoint Id.]: client="
                << GetClientId()
                << "; cached_endpoint_id_=" << cached_endpoint_id_;
            cached_endpoint_id_.clear();
          },
          kHighPowerAdvertisementEndpointIdCacheTimeout,
          &single_thread_executor_);
}

void ClientProxy::CancelClearCachedEndpointIdAlarm() {
  if (cached_endpoint_id_alarm_ && cached_endpoint_id_alarm_->IsValid()) {
    cached_endpoint_id_alarm_->Cancel();
    cached_endpoint_id_alarm_.reset();
  }
}

OsInfo::OsType ClientProxy::OSNameToOsInfoType(api::OSName osName) {
  switch (osName) {
    case api::OSName::kLinux:
      return OsInfo::LINUX;
    case api::OSName::kWindows:
      return OsInfo::WINDOWS;
    case api::OSName::kApple:
      return OsInfo::APPLE;
    case api::OSName::kChromeOS:
      return OsInfo::CHROME_OS;
    case api::OSName::kAndroid:
      return OsInfo::ANDROID;
  }
}

std::int32_t ClientProxy::GetLocalMultiplexSocketBitmask() const {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableMultiplex)) {
    std::int32_t multiplex_bitmask =
        kBtMultiplexEnabled | kWifiLanMultiplexEnabled;
    NEARBY_LOGS(INFO) << "ClientProxy [GetLocalMultiplexSocketBitmask]: "
                      << multiplex_bitmask;
    return multiplex_bitmask;
  }
  return 0;
}

void ClientProxy::SetRemoteMultiplexSocketBitmask(
    absl::string_view endpoint_id, int remote_multiplex_socket_bitmask) {
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.remote_multiplex_socket_bitmask =
        remote_multiplex_socket_bitmask;
    NEARBY_LOGS(INFO) << "ClientProxy [SetRemoteMultiplexSocketBitmask]: "
                      << remote_multiplex_socket_bitmask;
  }
}

bool ClientProxy::IsLocalMultiplexSocketSupported(Medium medium) {
  int bitmask = GetLocalMultiplexSocketBitmask();
  switch (medium) {
    case Medium::BLUETOOTH:
      NEARBY_LOGS(INFO) << "ClientProxy [IsLocalMultiplexSocketSupported]: "
                        << (bitmask & kBtMultiplexEnabled);
      return (bitmask & kBtMultiplexEnabled) != 0;
    case Medium::WIFI_LAN:
      return (bitmask & kWifiLanMultiplexEnabled) != 0;
    default:
      return false;
  }
}

std::optional<std::int32_t> ClientProxy::GetRemoteMultiplexSocketBitmask(
    absl::string_view endpoint_id) const {
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.remote_multiplex_socket_bitmask;
  }
  return std::nullopt;
}

bool ClientProxy::IsMultiplexSocketSupported(absl::string_view endpoint_id,
                                             Medium medium) {
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item == nullptr) {
    return false;
  }
  int combined_result = GetLocalMultiplexSocketBitmask() &
                        item->first.remote_multiplex_socket_bitmask;

  switch (medium) {
    case Medium::BLUETOOTH:
      return (combined_result & kBtMultiplexEnabled) != 0;
    case Medium::WIFI_LAN:
      return (combined_result & kWifiLanMultiplexEnabled) != 0;
    default:
      return false;
  }
}

bool ClientProxy::GetWebRtcNonCellular() { return webrtc_non_cellular_; }

void ClientProxy::SetWebRtcNonCellular(bool webrtc_non_cellular) {
  std::string allow_webrtc_cellular_str =
      webrtc_non_cellular ? "disallow" : "allow";
  NEARBY_LOGS(INFO) << "ClientProxy: client=" << GetClientId()
                    << allow_webrtc_cellular_str << " to use mobile data.",
      webrtc_non_cellular_ = webrtc_non_cellular;
}

std::string ClientProxy::ToString(PayloadProgressInfo::Status status) const {
  switch (status) {
    case PayloadProgressInfo::Status::kSuccess:
      return std::string("Success");
    case PayloadProgressInfo::Status::kFailure:
      return std::string("Failure");
    case PayloadProgressInfo::Status::kInProgress:
      return std::string("In Progress");
    case PayloadProgressInfo::Status::kCanceled:
      return std::string("Cancelled");
  }
}

std::string ClientProxy::Dump() {
  std::stringstream sstream;
  sstream << "Nearby Connections State" << std::endl;
  sstream << "  Client ID: " << GetClientId() << std::endl;
  sstream << "  Local Endpoint ID: " << GetLocalEndpointId() << std::endl;
  sstream << std::boolalpha;
  sstream << "  High Visibility Mode: " << high_vis_mode_ << std::endl;
  sstream << "  Is Advertising: " << IsAdvertising() << std::endl;
  sstream << "  Is Discovering: " << IsDiscovering() << std::endl;
  sstream << std::noboolalpha;
  sstream << "  Advertising Service ID: " << GetAdvertisingServiceId()
          << std::endl;
  sstream << "  Discovery Service ID: " << GetDiscoveryServiceId() << std::endl;
  sstream << "  Connections: " << std::endl;
  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    // TODO(deling): write Connection.ToString()
    sstream << "    " << it->first << " :(connection token) "
            << it->second.first.connection_token << ", (remote os type) "
            << (it->second.first.os_info.has_value()
                    ? location::nearby::connections::OsInfo::OsType_Name(
                          it->second.first.os_info->type())
                    : "unknown")
            << std::endl;
  }

  sstream << "  Discovered endpoint IDs: " << std::endl;
  for (auto it = discovered_endpoint_ids_.begin();
       it != discovered_endpoint_ids_.end(); ++it) {
    sstream << "    " << *it << std::endl;
  }

  return sstream.str();
}

}  // namespace connections
}  // namespace nearby
