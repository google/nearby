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
#include "absl/random/random.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/advertisements/dct_advertisement.h"
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
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/device_info_impl.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/app_lifecycle_monitor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/os_name.h"
#include "internal/platform/prng.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::connections::MediumRole;
using ::location::nearby::connections::OsInfo;

constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

constexpr absl::string_view kPreferencesFilePath = "Google/Nearby/Connections";

constexpr absl::string_view kAdvertisingEndpointId =
    "nc.advertising.endpoint_id";

constexpr absl::string_view kAdvertisingTimestamp = "nc.advertising.timestamp";

constexpr absl::Duration kAdvertisingKeepAliveDuration = absl::Seconds(30);

}  // namespace

ClientProxy::ClientProxy(::nearby::analytics::EventLogger* event_logger)
    : client_id_(Prng().NextInt64()) {
  VLOG(1) << "ClientProxy ctor event_logger=" << event_logger;
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableNearbyConnectionsPreferences)) {
    InitializePreferencesManager();
  }

  is_dct_enabled_ = NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_connections_feature::kEnableDct);
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
  support_auto_reconnect_ = false;
  local_safe_to_disconnect_version_ = NearbyFlags::GetInstance().GetInt64Flag(
      config_package_nearby::nearby_connections_feature::
          kSafeToDisconnectVersion);
  LOG(INFO) << "[safe-to-disconnect]: Local enabled: "
            << supports_safe_to_disconnect_
            << "; Version: " << local_safe_to_disconnect_version_;
  // Generate a 7 bits dedup value.
  absl::BitGen bitgen;
  dct_dedup_ = absl::Uniform(bitgen, 0, 1 << 7);

  // Load advertising info from preferences.
  LoadClientInfoFromPreferences();

  if (preferences_manager_ != nullptr) {
    app_lifecycle_monitor_ =
        api::ImplementationPlatform::CreateAppLifecycleMonitor(
            [this](api::AppLifecycleMonitor::AppLifecycleState state) {
              if (state ==
                      api::AppLifecycleMonitor::AppLifecycleState::kInactive ||
                  state == api::AppLifecycleMonitor::AppLifecycleState::
                               kBackground) {
                SaveClientInfoToPreferences();
              }
            });
  }
}

ClientProxy::~ClientProxy() { Reset(); }

std::int64_t ClientProxy::GetClientId() const { return client_id_; }

std::string ClientProxy::GetLocalEndpointId() {
  MutexLock lock(&mutex_);
  if (IsDctEnabled() && GetEndpointIdForDct().has_value()) {
    LOG(INFO) << "DCT is using genereted endpoint id.";
    return GetEndpointIdForDct().value();
  } else {
    if (!local_endpoint_id_.empty()) {
      LOG(INFO) << __func__
                << ": Reusing cached endpoint id: " << local_endpoint_id_;
      return local_endpoint_id_;
    }
    if (external_device_provider_ == nullptr) {
      local_endpoint_id_ = GenerateLocalEndpointId();
    } else {
      local_endpoint_id_ =
          external_device_provider_->GetLocalDevice()->GetEndpointId();
      LOG(INFO) << __func__
                << ": From external device provider, populating endpoint id: "
                << local_endpoint_id_;
    }
    return local_endpoint_id_;
  }
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

bool ClientProxy::OverrideSavePath(absl::string_view endpoint_id,
                                   absl::string_view path) {
  MutexLock lock(&mutex_);
  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.save_path = path;
    return true;
  }
  return false;
}

std::string ClientProxy::GetSavePath(
    absl::string_view endpoint_id) const {
  MutexLock lock(&mutex_);
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.save_path;
  }
  return "";
}

std::optional<MacAddress> ClientProxy::GetBluetoothMacAddress(
    const std::string& endpoint_id) {
  auto item = bluetooth_mac_addresses_.find(endpoint_id);
  if (item != bluetooth_mac_addresses_.end()) return item->second;
  return std::nullopt;
}

void ClientProxy::SetBluetoothMacAddress(const std::string& endpoint_id,
                                         MacAddress bluetooth_mac_address) {
  bluetooth_mac_addresses_[endpoint_id] = bluetooth_mac_address;
}

std::string ClientProxy::GenerateLocalEndpointId() {
  if (!cached_endpoint_id_.empty()) {
    if (stable_endpoint_id_mode_) {
      LOG(INFO) << "ClientProxy [Local Endpoint Re-using cached "
                    "endpoint id due to in stable endpoint id mode]: "
                    "client="
                << GetClientId()
                << "; cached_endpoint_id_=" << cached_endpoint_id_;
      return cached_endpoint_id_;
    }
  }
  std::string id;
  Prng prng;
  for (int i = 0; i < kEndpointIdLength; i++) {
    id += kEndpointIdChars[prng.NextUint32() % sizeof(kEndpointIdChars)];
  }
  LOG(INFO) << "ClientProxy [Local Endpoint Generated]: client="
            << GetClientId() << "; endpoint_id=" << id;
  return id;
}

void ClientProxy::Reset() {
  MutexLock lock(&mutex_);

  StoppedAdvertising();
  StoppedDiscovery();
  RemoveAllEndpoints();
  ExitStableEndpointIdMode();
}

void ClientProxy::StartedAdvertising(
    const std::string& service_id, Strategy strategy,
    const ConnectionListener& listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const std::vector<ConnectionsLog::OperationResultWithMedium>&
        operation_result_with_mediums,
    const AdvertisingOptions& advertising_options) {
  MutexLock lock(&mutex_);
  LOG(INFO) << "ClientProxy [StartedAdvertising]: client=" << GetClientId();

  if (stable_endpoint_id_mode_) {
    cached_endpoint_id_ = local_endpoint_id_;
  } else {
    cached_endpoint_id_.clear();
  }

  CancelClearCachedEndpointIdAlarm();

  advertising_info_ = {service_id, listener};
  advertising_options_ = advertising_options;

  const std::vector<location::nearby::proto::connections::Medium> medium_vector(
      mediums.begin(), mediums.end());
  std::unique_ptr<AdvertisingMetadataParams> advertising_metadata_params;
  advertising_metadata_params =
      GetAnalyticsRecorder().BuildAdvertisingMetadataParams();
  advertising_metadata_params->operation_result_with_mediums =
      std::move(operation_result_with_mediums);
  analytics_recorder_->OnStartAdvertising(strategy, medium_vector,
                                          advertising_metadata_params.get());
}

void ClientProxy::StoppedAdvertising() {
  MutexLock lock(&mutex_);
  LOG(INFO) << "ClientProxy [StoppedAdvertising]: client=" << GetClientId();

  if (IsAdvertising()) {
    advertising_info_.Clear();
    analytics_recorder_->OnStopAdvertising();
    local_endpoint_info_.clear();
  }
  // advertising_options_ is purposefully not cleared here.
  OnSessionComplete();

  ExitStableEndpointIdMode();
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
    const std::vector<ConnectionsLog::OperationResultWithMedium>&
        operation_result_with_mediums,
    const DiscoveryOptions& discovery_options) {
  MutexLock lock(&mutex_);
  discovery_info_ = DiscoveryInfo{service_id, std::move(listener)};
  discovery_options_ = discovery_options;

  const std::vector<location::nearby::proto::connections::Medium> medium_vector(
      mediums.begin(), mediums.end());
  std::unique_ptr<DiscoveryMetadataParams> discovery_metadata_params;
  discovery_metadata_params =
      GetAnalyticsRecorder().BuildDiscoveryMetadataParams();
  discovery_metadata_params->operation_result_with_mediums =
      std::move(operation_result_with_mediums);
  analytics_recorder_->OnStartDiscovery(strategy, medium_vector,
                                        discovery_metadata_params.get());
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

  LOG(INFO) << "ClientProxy [Endpoint Found]: [enter] id=" << endpoint_id
            << "; service=" << service_id
            << "; info=" << absl::BytesToHexString(endpoint_info.data())
            << "; medium="
            << location::nearby::proto::connections::Medium_Name(medium);
  if (!IsDiscoveringServiceId(service_id)) {
    LOG(INFO) << "ClientProxy [Endpoint Found]: Ignoring event for id="
              << endpoint_id << " because this client is not discovering.";
    return;
  }

  if (discovered_endpoint_ids_.count(endpoint_id)) {
    LOG(WARNING)
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

  LOG(INFO) << "ClientProxy [Endpoint Lost]: [enter] id=" << endpoint_id
            << "; service=" << service_id;
  if (!IsDiscoveringServiceId(service_id)) {
    LOG(INFO) << "ClientProxy [Endpoint Lost]: Ignoring event for id="
              << endpoint_id << " because this client is not discovering.";
    return;
  }

  const auto it = discovered_endpoint_ids_.find(endpoint_id);
  if (it == discovered_endpoint_ids_.end()) {
    LOG(WARNING)
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
  LOG(INFO) << "ClientProxy [RequestConnection]: id=" << endpoint_id;
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
  LOG(INFO) << "ClientProxy [Connection Initiated]: add Connection: client="
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
  LOG(INFO) << "ClientProxy [ConnectionAccepted]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    LOG(INFO) << "ClientProxy [Connection Accepted]: no pending "
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
  LOG(INFO) << "ClientProxy [ConnectionRejected]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    LOG(INFO) << "ClientProxy [Connection Rejected]: no pending "
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
  LOG(INFO) << "ClientProxy [BandwidthChanged]: id=" << endpoint_id;
  MutexLock lock(&mutex_);

  ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->first.connected_medium = new_medium;
    item->first.connection_listener.bandwidth_changed_cb(endpoint_id,
                                                         new_medium);
    LOG(INFO) << "ClientProxy [reporting onBandwidthChanged]: client="
              << GetClientId() << "; endpoint_id=" << endpoint_id;
  }
}

void ClientProxy::OnDisconnected(const std::string& endpoint_id, bool notify) {
  LOG(INFO) << "ClientProxy [OnDisconnected]: id=" << endpoint_id;
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

  if (!stable_endpoint_id_mode_ && !HasOngoingConnection()) {
    ScheduleClearCachedEndpointIdAlarm();
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

Medium ClientProxy::GetConnectedMedium(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connected_medium;
  }

  return Medium::UNKNOWN_MEDIUM;
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
    LOG(INFO)
        << "ClientProxy [Local Accepted]: local endpoint has responded; id="
        << endpoint_id;
    return;
  }
  AppendConnectionStatus(endpoint_id, Connection::kLocalEndpointAccepted);
  LOG(INFO) << "ClientProxy [Local Accepted]: id=" << endpoint_id;
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
    LOG(INFO)
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
    LOG(INFO)
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
    LOG(INFO)
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

void ClientProxy::SetLocalOsType(
    const location::nearby::connections::OsInfo::OsType& os_type) {
  local_os_info_.set_type(os_type);
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
      LOG(INFO) << "ClientProxy [reporting onPayloadReceived]: client="
                << GetClientId() << "; endpoint_id=" << endpoint_id
                << " ; payload {id:" << payload.GetId()
                << ", type:" << payload.GetType() << "}";
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
        VLOG(1) << "ClientProxy [reporting onPayloadProgress]: client="
                << GetClientId() << "; endpoint_id=" << endpoint_id
                << "; payload_id=" << info.payload_id
                << ", payload_status=" << ToString(info.status);
      } else {
        LOG(INFO) << "ClientProxy [reporting onPayloadProgress]: client="
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

void ClientProxy::EnterStableEndpointIdMode() {
  MutexLock lock(&mutex_);
  VLOG(1) << "ClientProxy [EnterStableEndpointIdMode]: client="
          << GetClientId();

  stable_endpoint_id_mode_ = true;
}

void ClientProxy::ExitStableEndpointIdMode() {
  MutexLock lock(&mutex_);
  VLOG(1) << "ClientProxy [ExitStableEndpointIdMode]: client=" << GetClientId();

  stable_endpoint_id_mode_ = false;
  ScheduleClearCachedEndpointIdAlarm();
}

void ClientProxy::ScheduleClearCachedEndpointIdAlarm() {
  CancelClearCachedEndpointIdAlarm();

  if (cached_endpoint_id_.empty()) {
    VLOG(1) << "ClientProxy [There is no cached local high power "
               "advertising endpoint Id]: client="
            << GetClientId();
    return;
  }

  if (HasOngoingConnection()) {
    VLOG(1) << "ClientProxy [Handle clearing cached endpoint ID "
               "during disconnection]: client="
            << GetClientId();
    return;
  }

  // Schedule to clear cache high visibility mode advertisement endpoint id in
  // 30s.
  LOG(INFO) << "ClientProxy [High Visibility Mode Adv, Schedule to "
               "Clear Cache EndpointId]: client="
            << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
  cached_endpoint_id_alarm_ = std::make_unique<CancelableAlarm>(
      "clear_high_power_endpoint_id_cache",
      [this]() { ClearCachedLocalEndpointId(); },
      kHighPowerAdvertisementEndpointIdCacheTimeout, &single_thread_executor_);
}

void ClientProxy::CancelClearCachedEndpointIdAlarm() {
  if (cached_endpoint_id_alarm_ && cached_endpoint_id_alarm_->IsValid()) {
    cached_endpoint_id_alarm_->Cancel();
    cached_endpoint_id_alarm_.reset();
  }
}

void ClientProxy::ClearCachedLocalEndpointId() {
  MutexLock lock(&mutex_);
  LOG(INFO) << "ClientProxy [Cleared cached local endpoint Id.]: client="
            << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
  cached_endpoint_id_.clear();
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
        (NearbyFlags::GetInstance().GetBoolFlag(
             config_package_nearby::nearby_connections_feature::
                 kEnableMultiplexBluetooth)
             ? kBtMultiplexEnabled
             : 0) |
        (NearbyFlags::GetInstance().GetBoolFlag(
             config_package_nearby::nearby_connections_feature::
                 kEnableMultiplexWifiLan)
             ? kWifiLanMultiplexEnabled
             : 0);
    LOG(INFO) << "ClientProxy [GetLocalMultiplexSocketBitmask]: "
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
    LOG(INFO) << "ClientProxy [SetRemoteMultiplexSocketBitmask]: "
              << remote_multiplex_socket_bitmask;
  }
}

bool ClientProxy::IsLocalMultiplexSocketSupported(Medium medium) {
  int bitmask = GetLocalMultiplexSocketBitmask();
  switch (medium) {
    case Medium::BLUETOOTH:
      LOG(INFO) << "ClientProxy [IsLocalMultiplexSocketSupported]: "
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
  VLOG(1) << "ClientProxy: client=" << GetClientId()
          << (webrtc_non_cellular ? " disallow" : " allow")
          << " to use mobile data.";
  webrtc_non_cellular_ = webrtc_non_cellular;
}

bool ClientProxy::IsDctEnabled() const { return is_dct_enabled_; }

uint8_t ClientProxy::GetDctDedup() const { return dct_dedup_; }

void ClientProxy::UpdateDctDeviceName(absl::string_view device_name) {
  if (!dct_device_name_.empty() && dct_device_name_ != device_name) {
    // Need to update dedup value if device name is changed.
    absl::BitGen bitgen;
    dct_dedup_ = absl::Uniform(bitgen, 0, 1 << 7);
  }

  dct_device_name_ = device_name;

  // The DCT endpoint ID should be derived from device name and dedup value.
  std::optional<std::string> dct_endpoint_id =
      advertisements::ble::DctAdvertisement::GenerateEndpointId(
          dct_dedup_, dct_device_name_);
  if (dct_endpoint_id.has_value()) {
    dct_endpoint_id_ = *dct_endpoint_id;
  } else {
    dct_endpoint_id_.clear();
  }
}

std::optional<MediumRole> ClientProxy::GetMediumRole(
    absl::string_view endpoint_id) const {
  const ConnectionPair* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->first.connection_options.connection_info.medium_role;
  }
  return std::nullopt;
}

std::optional<std::string> ClientProxy::GetEndpointIdForDct() const {
  if (dct_endpoint_id_.empty()) {
    return std::nullopt;
  }

  return dct_endpoint_id_;
}

void ClientProxy::InitializePreferencesManager() {
  LOG(INFO) << "ClientProxy [InitializePreferencesManager]: client="
            << GetClientId();
  auto device_info_ = std::make_unique<nearby::DeviceInfoImpl>();

  FilePath preferences_path =
      device_info_->GetAppDataPath().append(FilePath(kPreferencesFilePath));

  if (!Files::FileExists(preferences_path)) {
    Files::CreateDirectories(preferences_path);
  }

  preferences_manager_ = api::ImplementationPlatform::CreatePreferencesManager(
      preferences_path.ToString());

  if (preferences_manager_ == nullptr) {
    LOG(ERROR) << "ClientProxy [Failed to initialize preferences manager]: "
                  "client="
               << GetClientId();
  }
}

void ClientProxy::SaveClientInfoToPreferences() {
  MutexLock lock(&mutex_);
  if (preferences_manager_ == nullptr) {
    return;
  }

  if (advertising_info_.IsEmpty()) {
    preferences_manager_->Remove(kAdvertisingEndpointId);
    preferences_manager_->Remove(kAdvertisingTimestamp);
    return;
  }

  preferences_manager_->SetString(kAdvertisingEndpointId, local_endpoint_id_);
  preferences_manager_->SetTime(kAdvertisingTimestamp,
                                SystemClock::ElapsedRealtime());

  LOG(INFO) << "ClientProxy [SaveClientInfoToPreferences]: client="
            << GetClientId() << "; local_endpoint_id_=" << local_endpoint_id_;
}

void ClientProxy::LoadClientInfoFromPreferences() {
  MutexLock lock(&mutex_);
  if (preferences_manager_ == nullptr) {
    return;
  }

  absl::Time last_advertising_time = preferences_manager_->GetTime(
      kAdvertisingTimestamp, absl::InfinitePast());
  if (SystemClock::ElapsedRealtime() - last_advertising_time <
      kAdvertisingKeepAliveDuration) {
    std::string endpoint_id =
        preferences_manager_->GetString(kAdvertisingEndpointId, "");
    if (!endpoint_id.empty() && endpoint_id.length() == kEndpointIdLength) {
      bool is_valid_endpoint_id = true;
      for (const auto& c : endpoint_id) {
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
          is_valid_endpoint_id = false;
          break;
        }
      }

      if (is_valid_endpoint_id) {
        local_endpoint_id_ = endpoint_id;
        cached_endpoint_id_ = local_endpoint_id_;
        LOG(INFO) << "ClientProxy [LoadClientInfoFromPreferences]: client="
                  << GetClientId()
                  << "; local_endpoint_id_=" << local_endpoint_id_;
      }
    }
  }

  preferences_manager_->Remove(kAdvertisingEndpointId);
  preferences_manager_->Remove(kAdvertisingTimestamp);
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

}  // namespace nearby::connections
