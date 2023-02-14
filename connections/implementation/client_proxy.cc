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

#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "internal/analytics/event_logger.h"
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

using ::location::nearby::connections::OsInfo;

// The definition is necessary before C++17.
constexpr absl::Duration
    ClientProxy::kHighPowerAdvertisementEndpointIdCacheTimeout;

constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

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
}

ClientProxy::~ClientProxy() { Reset(); }

std::int64_t ClientProxy::GetClientId() const { return client_id_; }

std::string ClientProxy::GetLocalEndpointId() {
  MutexLock lock(&mutex_);
  if (local_endpoint_id_.empty()) {
    local_endpoint_id_ = GenerateLocalEndpointId();
    NEARBY_LOGS(INFO) << "ClientProxy [Local Endpoint Generated]: client="
                      << GetClientId()
                      << "; endpoint_id=" << local_endpoint_id_;
  }
  return local_endpoint_id_;
}

std::string ClientProxy::GetConnectionToken(const std::string& endpoint_id) {
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_token;
  }
  return {};
}

std::string ClientProxy::GenerateLocalEndpointId() {
  if (high_vis_mode_) {
    if (!local_high_vis_mode_cache_endpoint_id_.empty()) {
      NEARBY_LOGS(INFO)
          << "ClientProxy [Local Endpoint Re-using cached endpoint id]: client="
          << GetClientId() << "; local_high_vis_mode_cache_endpoint_id_="
          << local_high_vis_mode_cache_endpoint_id_;
      return local_high_vis_mode_cache_endpoint_id_;
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
  ExitHighVisibilityMode();
}

void ClientProxy::StartedAdvertising(
    const std::string& service_id, Strategy strategy,
    const ConnectionListener& listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const AdvertisingOptions& advertising_options) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "ClientProxy [StartedAdvertising]: client="
                    << GetClientId();

  if (high_vis_mode_) {
    local_high_vis_mode_cache_endpoint_id_ = local_endpoint_id_;
    NEARBY_LOGS(INFO)
        << "ClientProxy [High Visibility Mode Adv, Cache EndpointId]: client="
        << GetClientId() << "; local_high_vis_mode_cache_endpoint_id_="
        << local_high_vis_mode_cache_endpoint_id_;
    CancelClearLocalHighVisModeCacheEndpointIdAlarm();
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
  }
  // advertising_options_ is purposefully not cleared here.
  OnSessionComplete();

  ExitHighVisibilityMode();
}

bool ClientProxy::IsAdvertising() const {
  MutexLock lock(&mutex_);

  return !advertising_info_.IsEmpty();
}

std::string ClientProxy::GetAdvertisingServiceId() const {
  MutexLock lock(&mutex_);
  return advertising_info_.service_id;
}

void ClientProxy::StartedDiscovery(
    const std::string& service_id, Strategy strategy,
    const DiscoveryListener& listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const DiscoveryOptions& discovery_options) {
  MutexLock lock(&mutex_);
  discovery_info_ = DiscoveryInfo{service_id, listener};
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
                    << endpoint_id << "; service=" << service_id << "; info="
                    << absl::BytesToHexString(endpoint_info.data());
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
    NEARBY_LOG(INFO,
               "ClientProxy [Endpoint Lost]: Ignoring event for id=%s because "
               "this client is not discovering",
               endpoint_id.c_str());
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

void ClientProxy::OnConnectionInitiated(
    const std::string& endpoint_id, const ConnectionResponseInfo& info,
    const ConnectionOptions& connection_options,
    const ConnectionListener& listener, const std::string& connection_token) {
  MutexLock lock(&mutex_);

  // Whether this is incoming or outgoing, the local and remote endpoints both
  // still need to accept this connection, so set its establishment status to
  // PENDING.
  auto result = connections_.emplace(
      endpoint_id, Connection{
                       .is_incoming = info.is_incoming_connection,
                       .connection_listener = listener,
                       .connection_options = connection_options,
                       .connection_token = connection_token,
                   });
  // Instead of using structured binding which is nice, but banned
  // (can not use c++17 features, until chromium does) we unpack manually.
  auto& pair_iter = result.first;
  bool inserted = result.second;
  NEARBY_LOGS(INFO)
      << "ClientProxy [Connection Initiated]: add Connection: client="
      << GetClientId() << "; endpoint_id=" << endpoint_id
      << "; inserted=" << inserted;
  DCHECK(inserted);
  const Connection& item = pair_iter->second;
  // Notify the client.
  //
  // Note: we allow devices to connect to an advertiser even after it stops
  // advertising, so no need to check IsAdvertising() here.
  item.connection_listener.initiated_cb(endpoint_id, info);

  if (info.is_incoming_connection) {
    // Add CancellationFlag for advertisers once encryption succeeds.
    AddCancellationFlag(endpoint_id);
    analytics_recorder_->OnConnectionRequestReceived(endpoint_id);
  } else {
    analytics_recorder_->OnConnectionRequestSent(endpoint_id);
  }
}

void ClientProxy::OnConnectionAccepted(const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Connection Accepted]: no pending "
                         "connection; endpoint_id="
                      << endpoint_id;
    return;
  }

  // Notify the client.
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->connection_listener.accepted_cb(endpoint_id);
    item->status = Connection::kConnected;
  }
}

void ClientProxy::OnConnectionRejected(const std::string& endpoint_id,
                                       const Status& status) {
  MutexLock lock(&mutex_);

  if (!HasPendingConnectionToEndpoint(endpoint_id)) {
    NEARBY_LOGS(INFO) << "ClientProxy [Connection Rejected]: no pending "
                         "connection; endpoint_id="
                      << endpoint_id;
    return;
  }

  // Notify the client.
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->connection_listener.rejected_cb(endpoint_id, status);
    OnDisconnected(endpoint_id, false /* notify */);
  }
}

void ClientProxy::OnBandwidthChanged(const std::string& endpoint_id,
                                     Medium new_medium) {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->connection_listener.bandwidth_changed_cb(endpoint_id, new_medium);
    NEARBY_LOGS(INFO) << "ClientProxy [reporting onBandwidthChanged]: client="
                      << GetClientId() << "; endpoint_id=" << endpoint_id;
  }
}

void ClientProxy::OnDisconnected(const std::string& endpoint_id, bool notify) {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    if (notify) {
      item->connection_listener.disconnected_cb({endpoint_id});
    }
    connections_.erase(endpoint_id);
    OnSessionComplete();
  }

  CancelEndpoint(endpoint_id);
}

bool ClientProxy::ConnectionStatusMatches(const std::string& endpoint_id,
                                          Connection::Status status) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->status == status;
  }
  return false;
}

BooleanMediumSelector ClientProxy::GetUpgradeMediums(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.allowed;
  }
  return {};
}

bool ClientProxy::Is5GHzSupported(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.supports_5_ghz;
  }
  return false;
}

std::string ClientProxy::GetBssid(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.bssid;
  }
  return {};
}

std::int32_t ClientProxy::GetApFrequency(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.ap_frequency;
  }
  return -1;
}

std::string ClientProxy::GetIPAddress(const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.ip_address;
  }
  return {};
}

bool ClientProxy::IsConnectedToEndpoint(const std::string& endpoint_id) const {
  return ConnectionStatusMatches(endpoint_id, Connection::kConnected);
}

std::vector<std::string> ClientProxy::GetMatchingEndpoints(
    std::function<bool(const Connection&)> pred) const {
  MutexLock lock(&mutex_);

  std::vector<std::string> connected_endpoints;

  for (const auto& pair : connections_) {
    const auto& endpoint_id = pair.first;
    const auto& connection = pair.second;
    if (pred(connection)) {
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

bool ClientProxy::HasPendingConnectionToEndpoint(
    const std::string& endpoint_id) const {
  MutexLock lock(&mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->status != Connection::kConnected;
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
    const std::string& endpoint_id, const PayloadListener& listener) {
  MutexLock lock(&mutex_);

  if (HasLocalEndpointResponded(endpoint_id)) {
    NEARBY_LOGS(INFO)
        << "ClientProxy [Local Accepted]: local endpoint has responded; id="
        << endpoint_id;
    return;
  }

  AppendConnectionStatus(endpoint_id, Connection::kLocalEndpointAccepted);
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->payload_listener = listener;
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

void ClientProxy::AddCancellationFlag(const std::string& endpoint_id) {
  // Don't insert the CancellationFlag to the map if feature flag is disabled.
  if (!FeatureFlags::GetInstance().GetFlags().enable_cancellation_flag) {
    return;
  }

  auto item = cancellation_flags_.find(endpoint_id);
  if (item != cancellation_flags_.end()) {
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
  if (item == cancellation_flags_.end()) return;
  item->second->Cancel();
  cancellation_flags_.erase(item);
}

const OsInfo& ClientProxy::GetLocalOsInfo() const {
  return local_os_info_;
}

std::optional<OsInfo> ClientProxy::GetRemoteOsInfo(
    absl::string_view endpoint_id) const {
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->os_info;
  }
  return std::nullopt;
}

void ClientProxy::SetRemoteOsInfo(absl::string_view endpoint_id,
                                  const OsInfo& remote_os_info) {
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->os_info.emplace(remote_os_info);
  }
}
void ClientProxy::CancelAllEndpoints() {
  for (const auto& item : cancellation_flags_) {
    CancellationFlag* cancellation_flag = item.second.get();
    if (cancellation_flag->Cancelled()) {
      continue;
    }
    cancellation_flag->Cancel();
  }
  cancellation_flags_.clear();
}

void ClientProxy::OnPayload(const std::string& endpoint_id, Payload payload) {
  MutexLock lock(&mutex_);

  if (IsConnectedToEndpoint(endpoint_id)) {
    const Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      NEARBY_LOGS(INFO) << "ClientProxy [reporting onPayloadReceived]: client="
                        << GetClientId() << "; endpoint_id=" << endpoint_id
                        << " ; payload_id=" << payload.GetId();
      item->payload_listener.payload_cb(endpoint_id, std::move(payload));
    }
  }
}

const ClientProxy::Connection* ClientProxy::LookupConnection(
    absl::string_view endpoint_id) const {
  auto item = connections_.find(endpoint_id);
  return item != connections_.end() ? &item->second : nullptr;
}

ClientProxy::Connection* ClientProxy::LookupConnection(
    absl::string_view endpoint_id) {
  auto item = connections_.find(endpoint_id);
  return item != connections_.end() ? &item->second : nullptr;
}

void ClientProxy::OnPayloadProgress(const std::string& endpoint_id,
                                    const PayloadProgressInfo& info) {
  MutexLock lock(&mutex_);

  if (IsConnectedToEndpoint(endpoint_id)) {
    Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      item->payload_listener.payload_progress_cb(endpoint_id, info);

      if (info.status == PayloadProgressInfo::Status::kInProgress) {
        NEARBY_LOGS(VERBOSE)
            << "ClientProxy [reporting onPayloadProgress]: client="
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
  OnSessionComplete();
}

void ClientProxy::OnSessionComplete() {
  MutexLock lock(&mutex_);
  if (connections_.empty() && !IsAdvertising() && !IsDiscovering()) {
    local_endpoint_id_.clear();

    analytics_recorder_->LogSession();
    analytics_recorder_->LogStartSession();
  }
}

bool ClientProxy::ConnectionStatusesContains(
    const std::string& endpoint_id, Connection::Status status_to_match) const {
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return (item->status & status_to_match) != 0;
  }
  return false;
}

void ClientProxy::AppendConnectionStatus(const std::string& endpoint_id,
                                         Connection::Status status_to_append) {
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->status =
        static_cast<Connection::Status>(item->status | status_to_append);
  }
}

AdvertisingOptions ClientProxy::GetAdvertisingOptions() const {
  return advertising_options_;
}

DiscoveryOptions ClientProxy::GetDiscoveryOptions() const {
  return discovery_options_;
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
  ScheduleClearLocalHighVisModeCacheEndpointIdAlarm();
}

void ClientProxy::ScheduleClearLocalHighVisModeCacheEndpointIdAlarm() {
  CancelClearLocalHighVisModeCacheEndpointIdAlarm();

  if (local_high_vis_mode_cache_endpoint_id_.empty()) {
    NEARBY_LOGS(VERBOSE) << "ClientProxy [There is no cached local high power "
                            "advertising endpoint Id]: client="
                         << GetClientId();
    return;
  }

  // Schedule to clear cache high visibility mode advertisement endpoint id in
  // 30s.
  NEARBY_LOGS(INFO) << "ClientProxy [High Visibility Mode Adv, Schedule to "
                       "Clear Cache EndpointId]: client="
                    << GetClientId()
                    << "; local_high_vis_mode_cache_endpoint_id_="
                    << local_high_vis_mode_cache_endpoint_id_;
  clear_local_high_vis_mode_cache_endpoint_id_alarm_ =
      std::make_unique<CancelableAlarm>(
          "clear_high_power_endpoint_id_cache",
          [this]() {
            MutexLock lock(&mutex_);
            NEARBY_LOGS(INFO)
                << "ClientProxy [Cleared cached local high power advertising "
                   "endpoint Id.]: client="
                << GetClientId() << "; local_high_vis_mode_cache_endpoint_id_="
                << local_high_vis_mode_cache_endpoint_id_;
            local_high_vis_mode_cache_endpoint_id_.clear();
          },
          kHighPowerAdvertisementEndpointIdCacheTimeout,
          &single_thread_executor_);
}

void ClientProxy::CancelClearLocalHighVisModeCacheEndpointIdAlarm() {
  if (clear_local_high_vis_mode_cache_endpoint_id_alarm_ &&
      clear_local_high_vis_mode_cache_endpoint_id_alarm_->IsValid()) {
    clear_local_high_vis_mode_cache_endpoint_id_alarm_->Cancel();
    clear_local_high_vis_mode_cache_endpoint_id_alarm_.reset();
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
  sstream << "  High Visibility Mode: " << high_vis_mode_ << std::endl;
  sstream << "  Is Advertising: " << IsAdvertising() << std::endl;
  sstream << "  Advertising Service ID: " << GetAdvertisingServiceId()
          << std::endl;
  // TODO(deling): AdvertisingOptions
  sstream << "  Is Discovering: " << IsDiscovering() << std::endl;
  sstream << "  Discovery Service ID: " << GetDiscoveryServiceId() << std::endl;
  // TODO(deling): DiscoveryOptions

  sstream << "  Connections: " << std::endl;
  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    // TODO(deling): write Connection.ToString()
    sstream << "    " << it->first << " :(connection token) "
            << it->second.connection_token << ", (remote os type) "
            << (it->second.os_info.has_value()
                    ? location::nearby::connections::OsInfo::OsType_Name(
                          it->second.os_info->type())
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
