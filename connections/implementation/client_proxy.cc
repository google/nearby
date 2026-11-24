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
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/implementation/analytics/operation_result_with_medium.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/advertisements/dct_advertisement.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/connections_device_provider.h"
#include "connections/v3/listeners.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/connection_info.h"
#ifndef NEARBY_CHROMIUM
#include "internal/platform/implementation/device_info.h"
#endif
#include "internal/platform/error_code_params.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/app_lifecycle_monitor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/os_name.h"
#include "internal/platform/prng.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {

namespace {
using ::location::nearby::connections::MediumRole;
using ::location::nearby::connections::OsInfo;
using ::nearby::analytics::AnalyticsRecorder;

constexpr char kEndpointIdChars[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

constexpr absl::string_view kPreferencesFilePath = "Google/Nearby/Connections";

constexpr absl::string_view kAdvertisingEndpointId =
    "nc.advertising.endpoint_id";

constexpr absl::string_view kAdvertisingTimestamp = "nc.advertising.timestamp";

constexpr absl::Duration kAdvertisingKeepAliveDuration = absl::Seconds(30);

class NoOpAnalyticsRecorder : public AnalyticsRecorder {
 public:
  NoOpAnalyticsRecorder() = default;
  ~NoOpAnalyticsRecorder() override = default;

  // Advertising phase
  void OnStartAdvertising(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium>& mediums,
      AdvertisingMetadataParams* advertising_metadata_params) override {}
  void OnStopAdvertising() override {}

  int GetNextAdvertisingUpdateIndex() override { return 0; }
  void UpdateAdvertisingPowerLevel(
      location::nearby::proto::connections::PowerLevel power_level) override {}

  // Connection listening
  void OnStartedIncomingConnectionListening(
      connections::Strategy strategy) override {}
  void OnStoppedIncomingConnectionListening() override {}

  // Discovery phase
  void OnStartDiscovery(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium>& mediums,
      DiscoveryMetadataParams* discovery_metadata_params) override {}
  void OnStopDiscovery() override {}

  int GetNextDiscoveryUpdateIndex() override { return 0; }
  void OnEndpointFound(
      location::nearby::proto::connections::Medium medium) override {}

  // Connection request
  void OnRequestConnection(const connections::Strategy& strategy,
                           const std::string& endpoint_id) override {}

  void OnConnectionRequestReceived(
      const std::string& remote_endpoint_id) override {}
  void OnConnectionRequestSent(const std::string& remote_endpoint_id) override {
  }
  void OnRemoteEndpointAccepted(
      const std::string& remote_endpoint_id) override {}
  void OnLocalEndpointAccepted(const std::string& remote_endpoint_id) override {
  }
  void OnRemoteEndpointRejected(
      const std::string& remote_endpoint_id) override {}
  void OnLocalEndpointRejected(const std::string& remote_endpoint_id) override {
  }

  // Connection attempt
  void OnIncomingConnectionAttempt(
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string& connection_token,
      ConnectionAttemptMetadataParams* connection_attempt_metadata_params)
      override {}
  void OnOutgoingConnectionAttempt(
      const std::string& remote_endpoint_id,
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string& connection_token,
      ConnectionAttemptMetadataParams* connection_attempt_metadata_params)
      override {}

  // Connection established
  void OnConnectionEstablished(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium medium,
      const std::string& connection_token) override {}
  void OnConnectionClosed(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::DisconnectionReason reason,
      nearby::analytics::SafeDisconnectionResult result) override {}

  // Payload
  void OnIncomingPayloadStarted(const std::string& endpoint_id,
                                std::int64_t payload_id,
                                connections::PayloadType type,
                                std::int64_t total_size_bytes) override {}
  void OnPayloadChunkReceived(const std::string& endpoint_id,
                              std::int64_t payload_id,
                              std::int64_t chunk_size_bytes) override {}
  void OnIncomingPayloadDone(
      const std::string& endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) override {}
  void OnOutgoingPayloadStarted(const std::vector<std::string>& endpoint_ids,
                                std::int64_t payload_id,
                                connections::PayloadType type,
                                std::int64_t total_size_bytes) override {}
  void OnPayloadChunkSent(const std::string& endpoint_id,
                          std::int64_t payload_id,
                          std::int64_t chunk_size_bytes) override {}
  void OnOutgoingPayloadDone(
      const std::string& endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) override {}

  // BandwidthUpgrade
  void OnBandwidthUpgradeStarted(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium from_medium,
      location::nearby::proto::connections::Medium to_medium,
      location::nearby::proto::connections::ConnectionAttemptDirection
          direction,
      const std::string& connection_token, bool is_role_switched) override {}
  void UpdateBwUpgradeNetworkInfo(const std::string& endpoint_id,
                                  int num_interfaces,
                                  int num_ipv6_only_interfaces) override {}
  void OnBandwidthUpgradeError(
      const std::string& endpoint_id,
      location::nearby::proto::connections::BandwidthUpgradeResult result,
      location::nearby::proto::connections::BandwidthUpgradeErrorStage
          error_stage,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) override {}
  void OnBandwidthUpgradeSuccess(const std::string& endpoint_id) override {}

  // Error Code
  void OnErrorCode(const ErrorCodeParams& params) override {}

  void LogStartSession() override {}
  void LogSession() override {}

  bool IsSessionLogged() override { return false; }

  location::nearby::proto::connections::OperationResultCategory
  GetOperationResultCategory(
      location::nearby::proto::connections::OperationResultCode result_code)
      override {
    return location::nearby::proto::connections::OperationResultCategory::
        CATEGORY_UNKNOWN;
  }

  void Sync() override {}
};

void CancelAlarm(std::unique_ptr<CancelableAlarm> alarm) {
  if (alarm != nullptr && alarm->IsValid()) {
    alarm->Cancel();
  }
}

void CancelAlarms(std::vector<std::unique_ptr<CancelableAlarm>> alarms) {
  for (auto& alarm : alarms) {
    CancelAlarm(std::move(alarm));
  }
}

OsInfo::OsType OSNameToOsInfoType(api::OSName os_name) {
  switch (os_name) {
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

absl::string_view ToString(PayloadProgressInfo::Status status) {
  switch (status) {
    case PayloadProgressInfo::Status::kSuccess:
      return "Success";
    case PayloadProgressInfo::Status::kFailure:
      return "Failure";
    case PayloadProgressInfo::Status::kInProgress:
      return "In Progress";
    case PayloadProgressInfo::Status::kCanceled:
      return "Cancelled";
  }
}

}  // namespace

ClientProxy::ClientProxy(std::unique_ptr<AnalyticsRecorder> analytics_recorder)
    : is_dct_enabled_(NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableDct)),
      client_id_(Prng().NextInt64()),
#ifndef NEARBY_CHROMIUM
      local_device_name_(
          api::ImplementationPlatform::CreateDeviceInfo()
              ->GetOsDeviceName()
              .value_or("")),
#endif
      supports_safe_to_disconnect_(NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableSafeToDisconnect)),
      analytics_recorder_(analytics_recorder != nullptr
                              ? std::move(analytics_recorder)
                              : std::make_unique<NoOpAnalyticsRecorder>()),
      error_code_recorder_(std::make_unique<ErrorCodeRecorder>(
          [this](const ErrorCodeParams& params) {
            analytics_recorder_->OnErrorCode(params);
          })) {
  LOG(INFO) << "[safe-to-disconnect]: Local enabled: "
            << supports_safe_to_disconnect_ << "; Version: "
            << NearbyFlags::GetInstance().GetInt64Flag(
                   config_package_nearby::nearby_connections_feature::
                       kSafeToDisconnectVersion);

  bool has_preferences_manager = false;
  {
    absl::MutexLock lock(mutex_);
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kEnableNearbyConnectionsPreferences)) {
      InitializePreferencesManager();
    }
    local_os_info_.set_type(
        OSNameToOsInfoType(api::ImplementationPlatform::GetCurrentOS()));
    // Generate a 7 bits dedup value.
    absl::BitGen bitgen;
    dct_dedup_ = absl::Uniform(bitgen, 0, 1 << 7);

    // Load advertising info from preferences.
    LoadClientInfoFromPreferences();
    has_preferences_manager = (preferences_manager_ != nullptr);
  }

  if (has_preferences_manager) {
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

ClientProxy::~ClientProxy() {
  app_lifecycle_monitor_.reset();
  Reset();
  std::unique_ptr<CancelableAlarm> alarm_to_cancel;
  {
    absl::MutexLock lock(mutex_);
    alarm_to_cancel = CancelClearCachedEndpointIdAlarmLocked();
  }
  CancelAlarm(std::move(alarm_to_cancel));
  single_thread_executor_.Shutdown();
}

std::string ClientProxy::GetLocalEndpointId() {
  absl::MutexLock lock(mutex_);
  return GetLocalEndpointIdLocked();
}

std::string ClientProxy::GetLocalEndpointIdLocked() {
  if (IsDctEnabled()) {
    std::optional<std::string> dct_endpoint_id = GetEndpointIdForDct();
    if (dct_endpoint_id.has_value()) {
      LOG(INFO) << "DCT is using genereted endpoint id.";
      return dct_endpoint_id.value();
    }
  }
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

const NearbyDevice* ClientProxy::GetLocalDevice() {
  absl::MutexLock lock(mutex_);
  if (external_device_provider_ == nullptr &&
      connections_device_provider_ == nullptr) {
    // TODO(b/285602283): Plug in actual endpoint info once available.
    connections_device_provider_ =
        std::make_unique<v3::ConnectionsDeviceProvider>(
            GetLocalEndpointIdLocked(), "V3 endpoint",
            std::vector<ConnectionInfoVariant>{});
  }
  return GetLocalDeviceProviderLocked()->GetLocalDevice();
}

std::string ClientProxy::GetConnectionToken(const std::string& endpoint_id) {
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_token;
  }
  return {};
}

bool ClientProxy::OverrideSavePath(absl::string_view endpoint_id,
                                   absl::string_view path) {
  absl::MutexLock lock(mutex_);
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->save_path = path;
    return true;
  }
  return false;
}

std::string ClientProxy::GetSavePath(absl::string_view endpoint_id) const {
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->save_path;
  }
  return "";
}

std::string ClientProxy::GenerateLocalEndpointId() {
  if (!cached_endpoint_id_.empty()) {
    if (stable_endpoint_id_mode_ || HasOngoingConnection()) {
      LOG(INFO) << "ClientProxy [Local Endpoint Re-using cached "
                   "endpoint id due to in stable endpoint id mode or having "
                   "ongoing connection]: "
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
  std::vector<std::unique_ptr<CancelableAlarm>> alarms_to_cancel;
  ConnectionListener removed_advertising_listener;
  std::shared_ptr<const DiscoveryListener> removed_discovery_listener;
  std::shared_ptr<const v3::ConnectionListener> removed_listening_listener;
  absl::flat_hash_map<std::string, Connection> removed_connections;
  absl::flat_hash_map<std::string, std::shared_ptr<CancellationFlag>>
      removed_flags;
  {
    absl::MutexLock lock(mutex_);

    StoppedAdvertisingLocked(alarms_to_cancel, removed_advertising_listener);
    StoppedDiscoveryLocked(removed_discovery_listener);
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kFixClientSessionLogging)) {
      StoppedListeningForIncomingConnectionsLocked(removed_listening_listener);
    }
    RemoveAllEndpointsLocked(removed_connections, removed_flags);
    ExitStableEndpointIdModeLocked(alarms_to_cancel);
  }
  CancelAlarms(std::move(alarms_to_cancel));
}

void ClientProxy::RegisterEventHandler(ClientEventHandler handler) {
  ClientEventHandler old_handler;
  {
    absl::MutexLock lock(event_handler_mutex_);
    old_handler = std::move(event_handler_);
    event_handler_ = std::move(handler);
  }
}

void ClientProxy::UnregisterEventHandler() {
  ClientEventHandler old_handler;
  {
    absl::MutexLock lock(event_handler_mutex_);
    old_handler = std::move(event_handler_);
    event_handler_ = nullptr;
  }
}

bool ClientProxy::OnEvent(const ClientEvent& event) {
  absl::MutexLock lock(event_handler_mutex_);
  if (!event_handler_) {
    return true;
  }
  bool result = event_handler_(event);
  LOG(INFO) << "ClientProxy [OnEvent]: client=" << GetClientId()
            << "; endpoint_id=" << event.endpoint_id
            << "; type_index=" << event.data.index() << "; result=" << result;
  return result;
}

void ClientProxy::StartedAdvertising(
    const std::string& service_id, Strategy strategy,
    const ConnectionListener& listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const std::vector<analytics::OperationResultWithMedium>&
        operation_result_with_mediums,
    const AdvertisingOptions& advertising_options,
    std::optional<bool> is_extended_advertisement_supported,
    std::optional<int> connected_ap_frequency,
    std::optional<bool> is_nfc_available) {
  std::unique_ptr<CancelableAlarm> alarm_to_cancel;
  ConnectionListener old_listener;
  {
    absl::MutexLock lock(mutex_);
    LOG(INFO) << "ClientProxy [StartedAdvertising]: client=" << GetClientId();

    if (stable_endpoint_id_mode_) {
      cached_endpoint_id_ = local_endpoint_id_;
    } else {
      cached_endpoint_id_.clear();
    }

    alarm_to_cancel = CancelClearCachedEndpointIdAlarmLocked();

    old_listener = std::move(advertising_info_.listener);
    advertising_info_ = {service_id, listener};
    advertising_options_ = advertising_options;

    const std::vector<location::nearby::proto::connections::Medium>
        medium_vector(mediums.begin(), mediums.end());
    std::unique_ptr<AdvertisingMetadataParams> advertising_metadata_params;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kOmitUnmeasuredAdvDisCapabilities)) {
      advertising_metadata_params =
          AnalyticsRecorder::BuildAdvertisingMetadataParams(
              is_extended_advertisement_supported, connected_ap_frequency,
              is_nfc_available, operation_result_with_mediums);
    } else {
      advertising_metadata_params =
          AnalyticsRecorder::BuildAdvertisingMetadataParams(
              /*is_extended_advertisement_supported=*/false,
              /*connected_ap_frequency=*/0, /*is_nfc_available=*/false,
              operation_result_with_mediums);
    }
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kFixClientSessionLogging)) {
      advertising_metadata_params->power_level =
          advertising_options.low_power
              ? location::nearby::proto::connections::PowerLevel::LOW_POWER
              : location::nearby::proto::connections::PowerLevel::HIGH_POWER;
    }
    analytics_recorder_->OnStartAdvertising(strategy, medium_vector,
                                            advertising_metadata_params.get());
  }
  CancelAlarm(std::move(alarm_to_cancel));
}

void ClientProxy::StoppedAdvertising() {
  std::vector<std::unique_ptr<CancelableAlarm>> alarms_to_cancel;
  ConnectionListener removed_listener;
  {
    absl::MutexLock lock(mutex_);
    StoppedAdvertisingLocked(alarms_to_cancel, removed_listener);
  }
  CancelAlarms(std::move(alarms_to_cancel));
}

void ClientProxy::StoppedAdvertisingLocked(
    std::vector<std::unique_ptr<CancelableAlarm>>& alarms_to_cancel,
    ConnectionListener& removed_advertising_listener) {
  LOG(INFO) << "ClientProxy [StoppedAdvertising]: client=" << GetClientId();

  const bool was_advertising = IsAdvertisingLocked();
  if (was_advertising) {
    removed_advertising_listener = std::move(advertising_info_.listener);
    advertising_info_.Clear();
    analytics_recorder_->OnStopAdvertising();
    local_endpoint_info_.clear();
  }
  // advertising_options_ is purposefully not cleared here.
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging) ||
      was_advertising) {
    OnSessionCompleteLocked();
  }

  ExitStableEndpointIdModeLocked(alarms_to_cancel);
}

bool ClientProxy::IsAdvertising() const {
  absl::MutexLock lock(mutex_);
  return IsAdvertisingLocked();
}

std::string ClientProxy::GetAdvertisingServiceId() const {
  absl::MutexLock lock(mutex_);
  return advertising_info_.service_id;
}

void ClientProxy::UpdateAdvertisingOptions(
    const AdvertisingOptions& advertising_options) {
  absl::MutexLock lock(mutex_);
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging) &&
      IsAdvertisingLocked() && advertising_options_.low_power &&
      !advertising_options.low_power) {
    analytics_recorder_->UpdateAdvertisingPowerLevel(
        location::nearby::proto::connections::PowerLevel::HIGH_POWER);
  }
  advertising_options_ = advertising_options;
}

void ClientProxy::StartedListeningForIncomingConnections(
    absl::string_view service_id, Strategy strategy,
    v3::ConnectionListener listener,
    const v3::ConnectionListeningOptions& options) {
  std::shared_ptr<const v3::ConnectionListener> old_listener;
  {
    absl::MutexLock lock(mutex_);
    listening_options_ = options;
    old_listener = std::move(listening_info_.listener);
    listening_info_ = ListeningInfo{
        .service_id = std::string(service_id),
        .listener =
            std::make_shared<const v3::ConnectionListener>(std::move(listener)),
    };
    analytics_recorder_->OnStartedIncomingConnectionListening(strategy);
  }
}

void ClientProxy::StoppedListeningForIncomingConnections() {
  std::shared_ptr<const v3::ConnectionListener> removed_listener;
  {
    absl::MutexLock lock(mutex_);
    StoppedListeningForIncomingConnectionsLocked(removed_listener);
  }
}

void ClientProxy::StoppedListeningForIncomingConnectionsLocked(
    std::shared_ptr<const v3::ConnectionListener>& removed_listening_listener) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging)) {
    if (IsListeningForIncomingConnectionsLocked()) {
      removed_listening_listener = std::move(listening_info_.listener);
      listening_info_.Clear();
      analytics_recorder_->OnStoppedIncomingConnectionListening();
      OnSessionCompleteLocked();
    }
    return;
  }
  removed_listening_listener = std::move(listening_info_.listener);
  listening_info_.Clear();
  analytics_recorder_->OnStoppedIncomingConnectionListening();
}

bool ClientProxy::IsListeningForIncomingConnections() const {
  absl::MutexLock lock(mutex_);
  return IsListeningForIncomingConnectionsLocked();
}

std::string ClientProxy::GetListeningForIncomingConnectionsServiceId() const {
  absl::MutexLock lock(mutex_);
  if (IsListeningForIncomingConnectionsLocked()) {
    return listening_info_.service_id;
  }
  return "";
}

ConnectionListener ClientProxy::GetAdvertisingOrIncomingConnectionListener() {
  absl::MutexLock lock(mutex_);
  if (IsListeningForIncomingConnectionsLocked() &&
       listening_info_.listener != nullptr) {
    std::shared_ptr<const v3::ConnectionListener> shared_listener =
        listening_info_.listener;
    return ConnectionListener{
        .initiated_cb =
            [shared_listener](const std::string& endpoint_id,
                              const ConnectionResponseInfo& info) {
              auto remote_device = v3::ConnectionsDevice(
                  endpoint_id, info.remote_endpoint_info.AsStringView(), {});
              shared_listener->initiated_cb(
                  remote_device,
                  v3::InitialConnectionInfo{
                      .authentication_digits = info.authentication_token,
                      .raw_authentication_token =
                          info.raw_authentication_token.string_data(),
                      .is_incoming_connection = info.is_incoming_connection,
                  });
            },
        .accepted_cb =
            [shared_listener](const std::string& endpoint_id) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              shared_listener->result_cb(
                  remote_device,
                  v3::ConnectionResult{.status = Status{
                                           .value = Status::kSuccess,
                                       }});
            },
        .rejected_cb =
            [shared_listener](const std::string& endpoint_id, Status status) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              shared_listener->result_cb(
                  remote_device, v3::ConnectionResult{.status = status});
            },
        .disconnected_cb =
            [shared_listener](const std::string& endpoint_id) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              shared_listener->disconnected_cb(remote_device);
            },
        .bandwidth_changed_cb =
            [shared_listener](const std::string& endpoint_id, Medium medium) {
              auto remote_device = v3::ConnectionsDevice(endpoint_id, "", {});
              shared_listener->bandwidth_changed_cb(
                  remote_device, v3::BandwidthInfo{.medium = medium});
            },
    };
  }
  return advertising_info_.listener;
}

void ClientProxy::StartedDiscovery(
    const std::string& service_id, Strategy strategy,
    DiscoveryListener listener,
    absl::Span<location::nearby::proto::connections::Medium> mediums,
    const std::vector<analytics::OperationResultWithMedium>&
        operation_result_with_mediums,
    const DiscoveryOptions& discovery_options,
    std::optional<bool> is_extended_advertisement_supported,
    std::optional<int> connected_ap_frequency,
    std::optional<bool> is_nfc_available) {
  std::shared_ptr<const DiscoveryListener> old_listener;
  {
    absl::MutexLock lock(mutex_);
    old_listener = std::move(discovery_info_.listener);
    discovery_info_ = DiscoveryInfo{
        service_id,
        std::make_shared<const DiscoveryListener>(std::move(listener)),
    };
    discovery_options_ = discovery_options;

    const std::vector<location::nearby::proto::connections::Medium>
        medium_vector(mediums.begin(), mediums.end());
    std::unique_ptr<DiscoveryMetadataParams> discovery_metadata_params;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kOmitUnmeasuredAdvDisCapabilities)) {
      discovery_metadata_params =
          AnalyticsRecorder::BuildDiscoveryMetadataParams(
              is_extended_advertisement_supported, connected_ap_frequency,
              is_nfc_available, operation_result_with_mediums);
    } else {
      discovery_metadata_params =
          AnalyticsRecorder::BuildDiscoveryMetadataParams(
              /*is_extended_advertisement_supported=*/false,
              /*connected_ap_frequency=*/0, /*is_nfc_available=*/false,
              operation_result_with_mediums);
    }
    analytics_recorder_->OnStartDiscovery(strategy, medium_vector,
                                          discovery_metadata_params.get());
  }
}

void ClientProxy::StoppedDiscovery() {
  std::shared_ptr<const DiscoveryListener> removed_listener;
  {
    absl::MutexLock lock(mutex_);
    StoppedDiscoveryLocked(removed_listener);
  }
}

void ClientProxy::StoppedDiscoveryLocked(
    std::shared_ptr<const DiscoveryListener>& removed_discovery_listener) {
  const bool was_discovering = IsDiscoveringLocked();
  if (was_discovering) {
    discovered_endpoint_ids_.clear();
    removed_discovery_listener = std::move(discovery_info_.listener);
    discovery_info_.Clear();
    analytics_recorder_->OnStopDiscovery();
  }
  // discovery_options_ is purposefully not cleared here.
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging) ||
      was_discovering) {
    OnSessionCompleteLocked();
  }
}

bool ClientProxy::IsDiscoveringServiceId(const std::string& service_id) const {
  absl::MutexLock lock(mutex_);
  return IsDiscoveringServiceIdLocked(service_id);
}

bool ClientProxy::IsDiscovering() const {
  absl::MutexLock lock(mutex_);
  return IsDiscoveringLocked();
}

std::string ClientProxy::GetDiscoveryServiceId() const {
  absl::MutexLock lock(mutex_);
  return discovery_info_.service_id;
}

void ClientProxy::OnEndpointFound(
    const std::string& service_id, const std::string& endpoint_id,
    const ByteArray& endpoint_info,
    location::nearby::proto::connections::Medium medium) {
  LOG(INFO) << "ClientProxy [Endpoint Found]: [enter] id=" << endpoint_id
            << "; service=" << service_id
            << "; info=" << absl::BytesToHexString(endpoint_info.data())
            << "; medium="
            << location::nearby::proto::connections::Medium_Name(medium);
  std::shared_ptr<const DiscoveryListener> listener;
  {
    absl::MutexLock lock(mutex_);
    if (!IsDiscoveringServiceIdLocked(service_id)) {
      LOG(INFO) << "ClientProxy [Endpoint Found]: Ignoring event for id="
                << endpoint_id << " because this client is not discovering.";
      return;
    }
    if (discovered_endpoint_ids_.contains(endpoint_id)) {
      LOG(WARNING)
          << "ClientProxy [Endpoint Found]: Ignoring event for id="
          << endpoint_id
          << " because this client has already reported this endpoint as "
             "found.";
      return;
    }
    discovered_endpoint_ids_.insert(endpoint_id);
    listener = discovery_info_.listener;
  }
  if (listener != nullptr) {
    listener->endpoint_found_cb(endpoint_id, endpoint_info, service_id);
    analytics_recorder_->OnEndpointFound(medium);
  }
}

void ClientProxy::OnEndpointLost(const std::string& service_id,
                                 const std::string& endpoint_id) {
  LOG(INFO) << "ClientProxy [Endpoint Lost]: [enter] id=" << endpoint_id
            << "; service=" << service_id;
  std::shared_ptr<const DiscoveryListener> listener;
  {
    absl::MutexLock lock(mutex_);
    if (!IsDiscoveringServiceIdLocked(service_id)) {
      LOG(INFO) << "ClientProxy [Endpoint Lost]: Ignoring event for id="
                << endpoint_id << " because this client is not discovering.";
      return;
    }
    const auto it = discovered_endpoint_ids_.find(endpoint_id);
    if (it == discovered_endpoint_ids_.end()) {
      LOG(WARNING)
          << "ClientProxy [Endpoint Lost]: Ignoring event for id="
          << endpoint_id
          << " because this client has not yet reported this endpoint as "
             "found";
      return;
    }
    discovered_endpoint_ids_.erase(it);
    listener = discovery_info_.listener;
  }
  if (listener != nullptr) {
    listener->endpoint_lost_cb(endpoint_id);
  }
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
  ConnectionListener listener_copy = listener;
  {
    absl::MutexLock lock(mutex_);

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
    bool inserted = result.second;
    LOG(INFO) << "ClientProxy [Connection Initiated]: add Connection: client="
              << GetClientId() << "; endpoint_id=" << endpoint_id
              << "; inserted=" << inserted;
    DCHECK(inserted);

    if (info.is_incoming_connection) {
      // Add CancellationFlag for advertisers once encryption succeeds.
      AddCancellationFlagLocked(endpoint_id);
    }
  }

  // Notify the client outside the lock so re-entrant calls into ClientProxy do
  // not deadlock.
  //
  // Note: we allow devices to connect to an advertiser even after it stops
  // advertising, so no need to check IsAdvertising() here.
  listener_copy.initiated_cb(endpoint_id, info);

  if (info.is_incoming_connection) {
    analytics_recorder_->OnConnectionRequestReceived(endpoint_id);
  } else {
    analytics_recorder_->OnConnectionRequestSent(endpoint_id);
  }
}

void ClientProxy::OnConnectionAccepted(const std::string& endpoint_id) {
  LOG(INFO) << "ClientProxy [ConnectionAccepted]: id=" << endpoint_id;
  std::function<void(const std::string&)> accepted_cb;
  {
    absl::MutexLock lock(mutex_);

    if (!HasPendingConnectionToEndpointLocked(endpoint_id)) {
      LOG(INFO) << "ClientProxy [Connection Accepted]: no pending "
                   "connection; endpoint_id="
                << endpoint_id;
      return;
    }

    Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      accepted_cb = item->connection_listener.accepted_cb;
      item->status = Connection::kConnected;
    }
  }

  if (accepted_cb) {
    accepted_cb(endpoint_id);
  }
}

void ClientProxy::OnConnectionRejected(const std::string& endpoint_id,
                                       const Status& status) {
  LOG(INFO) << "ClientProxy [ConnectionRejected]: id=" << endpoint_id;
  std::function<void(const std::string&, Status)> rejected_cb;
  {
    absl::MutexLock lock(mutex_);

    if (!HasPendingConnectionToEndpointLocked(endpoint_id)) {
      LOG(INFO) << "ClientProxy [Connection Rejected]: no pending "
                   "connection; endpoint_id="
                << endpoint_id;
      return;
    }

    const Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      rejected_cb = item->connection_listener.rejected_cb;
    }
  }

  if (rejected_cb) {
    rejected_cb(endpoint_id, status);
    OnDisconnected(endpoint_id, false /* notify */);
  }
}

void ClientProxy::OnBandwidthChanged(const std::string& endpoint_id,
                                     Medium new_medium) {
  LOG(INFO) << "ClientProxy [BandwidthChanged]: id=" << endpoint_id;
  std::function<void(const std::string&, Medium)> bandwidth_changed_cb;
  {
    absl::MutexLock lock(mutex_);

    Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      item->connected_medium = new_medium;
      bandwidth_changed_cb = item->connection_listener.bandwidth_changed_cb;
    }
  }

  if (bandwidth_changed_cb) {
    bandwidth_changed_cb(endpoint_id, new_medium);
    LOG(INFO) << "ClientProxy [reporting onBandwidthChanged]: client="
              << GetClientId() << "; endpoint_id=" << endpoint_id;
  }
}

void ClientProxy::OnDisconnected(const std::string& endpoint_id, bool notify) {
  LOG(INFO) << "ClientProxy [OnDisconnected]: id=" << endpoint_id;
  std::function<void(const std::string&)> disconnected_cb;
  std::optional<Connection> removed_connection;
  std::shared_ptr<CancellationFlag> cancellation_flag;
  std::unique_ptr<CancelableAlarm> alarm_to_cancel;
  {
    absl::MutexLock lock(mutex_);

    auto it = connections_.find(endpoint_id);
    if (it != connections_.end()) {
      if (notify) {
        disconnected_cb = it->second.connection_listener.disconnected_cb;
      }
      removed_connection = std::move(it->second);
      connections_.erase(it);
      OnSessionCompleteLocked();
    }

    cancellation_flag = GetCancellationFlagToCancelLocked(endpoint_id);

    if (!stable_endpoint_id_mode_ && !HasOngoingConnection()) {
      alarm_to_cancel = ScheduleClearCachedEndpointIdAlarmLocked();
    }
  }

  CancelAlarm(std::move(alarm_to_cancel));
  if (cancellation_flag != nullptr) {
    cancellation_flag->Cancel();
  }
  if (disconnected_cb) {
    disconnected_cb(endpoint_id);
  }
}

bool ClientProxy::IsConnectedToEndpointLocked(
    const std::string& endpoint_id) const {
  const Connection* item = LookupConnection(endpoint_id);
  return item != nullptr && item->status == Connection::kConnected;
}

Medium ClientProxy::GetConnectedMedium(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connected_medium;
  }

  return Medium::UNKNOWN_MEDIUM;
}

BooleanMediumSelector ClientProxy::GetUpgradeMediums(
    const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.allowed;
  }
  return {};
}

bool ClientProxy::Is5GHzSupported(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.supports_5_ghz;
  }
  return false;
}

std::string ClientProxy::GetBssid(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.bssid;
  }
  return {};
}

std::int32_t ClientProxy::GetApFrequency(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.ap_frequency;
  }
  return -1;
}

bool ClientProxy::IsConnectedToEndpoint(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);
  return IsConnectedToEndpointLocked(endpoint_id);
}

std::vector<std::string> ClientProxy::GetMatchingEndpoints(
    absl::AnyInvocable<bool(const Connection&)> pred) const {
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
  absl::MutexLock lock(mutex_);
  return GetMatchingEndpoints([](const Connection& connection) {
    return connection.status != Connection::kConnected;
  });
}

std::vector<std::string> ClientProxy::GetConnectedEndpoints() const {
  absl::MutexLock lock(mutex_);
  return GetMatchingEndpoints([](const Connection& connection) {
    return connection.status == Connection::kConnected;
  });
}

bool ClientProxy::HasWifiDirectConnection() const {
  for (const auto& entry : connections_) {
    if (entry.second.connected_medium == Medium::WIFI_DIRECT) {
      LOG(INFO) << "ClientProxy [HasWifiDirectConnection]: true";
      return true;
    }
  }
  LOG(INFO) << "ClientProxy [HasWifiDirectConnection]: false";
  return false;
}

bool ClientProxy::HasWifiHotspotConnection() const {
  for (const auto& entry : connections_) {
    if (entry.second.connected_medium == Medium::WIFI_HOTSPOT) {
      return true;
    }
  }
  return false;
}

bool ClientProxy::HasWifiAwareConnection() const {
  for (const auto& entry : connections_) {
    if (entry.second.connected_medium == Medium::WIFI_AWARE ||
        entry.second.connected_medium == Medium::WIFI_AWARE_R4) {
      return true;
    }
  }
  return false;
}

std::string ClientProxy::GetLastLocalEndpointId() const {
  absl::MutexLock lock(mutex_);
  return last_local_endpoint_id_;
}

std::int32_t ClientProxy::GetNumOutgoingConnections() const {
  absl::MutexLock lock(mutex_);
  int32_t count = 0;
  for (const auto& pair : connections_) {
    const auto& connection = pair.second;
    if (connection.status == Connection::kConnected &&
        !connection.is_incoming) {
      count++;
    }
  }
  return count;
}

std::int32_t ClientProxy::GetNumIncomingConnections() const {
  absl::MutexLock lock(mutex_);
  int32_t count = 0;
  for (const auto& pair : connections_) {
    const auto& connection = pair.second;
    if (connection.status == Connection::kConnected && connection.is_incoming) {
      count++;
    }
  }
  return count;
}

bool ClientProxy::IsIncomingConnection(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr && item->status == Connection::kConnected) {
    return item->is_incoming;
  }
  return false;
}

bool ClientProxy::HasPendingConnectionToEndpoint(
    const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);
  return HasPendingConnectionToEndpointLocked(endpoint_id);
}

bool ClientProxy::HasPendingConnectionToEndpointLocked(
    const std::string& endpoint_id) const {
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->status != Connection::kConnected;
  }
  return false;
}

bool ClientProxy::HasLocalEndpointResponded(
    const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);
  return HasLocalEndpointRespondedLocked(endpoint_id);
}

bool ClientProxy::HasLocalEndpointRespondedLocked(
    const std::string& endpoint_id) const {
  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kLocalEndpointAccepted |
                                      Connection::kLocalEndpointRejected));
}

bool ClientProxy::HasRemoteEndpointResponded(
    const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);
  return HasRemoteEndpointRespondedLocked(endpoint_id);
}

bool ClientProxy::HasRemoteEndpointRespondedLocked(
    const std::string& endpoint_id) const {
  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kRemoteEndpointAccepted |
                                      Connection::kRemoteEndpointRejected));
}

void ClientProxy::LocalEndpointAcceptedConnection(
    const std::string& endpoint_id, PayloadListener listener) {
  std::shared_ptr<const PayloadListener> old_listener;
  {
    absl::MutexLock lock(mutex_);
    if (HasLocalEndpointRespondedLocked(endpoint_id)) {
      LOG(INFO)
          << "ClientProxy [Local Accepted]: local endpoint has responded; id="
          << endpoint_id;
      return;
    }
    AppendConnectionStatus(endpoint_id, Connection::kLocalEndpointAccepted);
    LOG(INFO) << "ClientProxy [Local Accepted]: id=" << endpoint_id;
    Connection* item = LookupConnection(endpoint_id);
    if (item != nullptr) {
      old_listener = std::move(item->payload_listener);
      item->payload_listener =
          std::make_shared<const PayloadListener>(std::move(listener));
    }
    analytics_recorder_->OnLocalEndpointAccepted(endpoint_id);
  }
}

void ClientProxy::LocalEndpointRejectedConnection(
    const std::string& endpoint_id) {
  absl::MutexLock lock(mutex_);

  if (HasLocalEndpointRespondedLocked(endpoint_id)) {
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
  absl::MutexLock lock(mutex_);

  if (HasRemoteEndpointRespondedLocked(endpoint_id)) {
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
  absl::MutexLock lock(mutex_);

  if (HasRemoteEndpointRespondedLocked(endpoint_id)) {
    LOG(INFO)
        << "ClientProxy [Remote Rejected]: remote endpoint has responded; id="
        << endpoint_id;
    return;
  }

  AppendConnectionStatus(endpoint_id, Connection::kRemoteEndpointRejected);
  analytics_recorder_->OnRemoteEndpointRejected(endpoint_id);
}

bool ClientProxy::IsConnectionAccepted(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  return ConnectionStatusesContains(endpoint_id,
                                    Connection::kLocalEndpointAccepted) &&
         ConnectionStatusesContains(endpoint_id,
                                    Connection::kRemoteEndpointAccepted);
}

bool ClientProxy::IsConnectionRejected(const std::string& endpoint_id) const {
  absl::MutexLock lock(mutex_);

  return ConnectionStatusesContains(
      endpoint_id,
      static_cast<Connection::Status>(Connection::kLocalEndpointRejected |
                                      Connection::kRemoteEndpointRejected));
}

bool ClientProxy::AutoUpgradeBandwidth() const {
  absl::MutexLock lock(mutex_);
  bool result = false;
  if (IsAdvertisingLocked() && (advertising_options_.strategy.IsNone() ||
                                advertising_options_.auto_upgrade_bandwidth)) {
    result |= true;
  }
  if (IsListeningForIncomingConnectionsLocked() &&
      (listening_options_.strategy.IsNone() ||
       listening_options_.auto_upgrade_bandwidth)) {
    result |= true;
  }
  return result;
}

bool ClientProxy::ShouldEnforceTopologyConstraints() const {
  absl::MutexLock lock(mutex_);
  bool result = false;
  if (IsAdvertisingLocked() &&
      (advertising_options_.strategy.IsNone() ||
       advertising_options_.enforce_topology_constraints)) {
    result |= true;
  }
  if (IsListeningForIncomingConnectionsLocked() &&
      (listening_options_.strategy.IsNone() ||
       listening_options_.enforce_topology_constraints)) {
    result |= true;
  }
  return result;
}

void ClientProxy::AddCancellationFlag(const std::string& endpoint_id) {
  absl::MutexLock lock(mutex_);
  AddCancellationFlagLocked(endpoint_id);
}

void ClientProxy::AddCancellationFlagLocked(const std::string& endpoint_id) {
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
                              std::make_shared<CancellationFlag>());
}

std::shared_ptr<CancellationFlag> ClientProxy::GetCancellationFlag(
    const std::string& endpoint_id) {
  absl::MutexLock lock(mutex_);
  const auto item = cancellation_flags_.find(endpoint_id);
  if (item == cancellation_flags_.end()) {
    return default_cancellation_flag_;
  }
  return item->second;
}

std::shared_ptr<CancellationFlag>
ClientProxy::GetCancellationFlagToCancelLocked(const std::string& endpoint_id) {
  const auto item = cancellation_flags_.find(endpoint_id);
  if (item != cancellation_flags_.end()) {
    return item->second;
  }
  return nullptr;
}

void ClientProxy::CancelEndpoint(const std::string& endpoint_id) {
  std::shared_ptr<CancellationFlag> cancellation_flag;
  {
    absl::MutexLock lock(mutex_);
    cancellation_flag = GetCancellationFlagToCancelLocked(endpoint_id);
  }
  if (cancellation_flag != nullptr) {
    cancellation_flag->Cancel();
  }
}

OsInfo ClientProxy::GetLocalOsInfo() const {
  absl::MutexLock lock(mutex_);
  return local_os_info_;
}

std::optional<OsInfo> ClientProxy::GetRemoteOsInfo(
    absl::string_view endpoint_id) const {
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->os_info;
  }
  return std::nullopt;
}

void ClientProxy::SetLocalOsType(
    const location::nearby::connections::OsInfo::OsType& os_type) {
  absl::MutexLock lock(mutex_);
  local_os_info_.set_type(os_type);
}

void ClientProxy::SetRemoteOsInfo(absl::string_view endpoint_id,
                                  const OsInfo& remote_os_info) {
  absl::MutexLock lock(mutex_);
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->os_info.emplace(remote_os_info);
  }
}

void ClientProxy::SetRemoteDeviceName(absl::string_view endpoint_id,
                                      absl::string_view device_name) {
  absl::MutexLock lock(mutex_);
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->device_name = std::string(device_name);
    LOG(INFO) << "ClientProxy [SetRemoteDeviceName]: " << device_name;
  }
}

std::string ClientProxy::GetRemoteDeviceName(
    absl::string_view endpoint_id) const {
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->device_name;
  }
  return "";
}

std::optional<std::int32_t> ClientProxy::GetRemoteSafeToDisconnectVersion(
    absl::string_view endpoint_id) const {
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->safe_to_disconnect_version;
  }
  return std::nullopt;
}

void ClientProxy::SetRemoteSafeToDisconnectVersion(
    absl::string_view endpoint_id,
    const std::int32_t& safe_to_disconnect_version) {
  absl::MutexLock lock(mutex_);
  Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    item->safe_to_disconnect_version = safe_to_disconnect_version;
  }
}

bool ClientProxy::IsSafeToDisconnectEnabled(absl::string_view endpoint_id) {
  if (!IsSupportSafeToDisconnect()) {
    return false;
  }
  std::optional<std::int32_t> version;
  {
    absl::MutexLock lock(mutex_);
    version = GetRemoteSafeToDisconnectVersion(endpoint_id);
  }
  return version.has_value() &&
         (*version >= FeatureFlags::GetInstance()
                          .GetFlags()
                          .min_nc_version_supports_safe_to_disconnect);
}

void ClientProxy::CancelAllEndpoints() {
  std::vector<std::shared_ptr<CancellationFlag>> flags_to_cancel;
  {
    absl::MutexLock lock(mutex_);
    flags_to_cancel.reserve(cancellation_flags_.size());
    for (const auto& item : cancellation_flags_) {
      flags_to_cancel.push_back(item.second);
    }
  }
  for (const auto& cancellation_flag : flags_to_cancel) {
    if (cancellation_flag->Cancelled()) {
      continue;
    }
    cancellation_flag->Cancel();
  }
}

void ClientProxy::OnPayload(const std::string& endpoint_id, Payload payload) {
  std::shared_ptr<const PayloadListener> listener;
  {
    absl::MutexLock lock(mutex_);
    if (!IsConnectedToEndpointLocked(endpoint_id)) {
      return;
    }
    const Connection* item = LookupConnection(endpoint_id);
    if (item == nullptr || item->payload_listener == nullptr) {
      return;
    }
    listener = item->payload_listener;
  }

  LOG(INFO) << "ClientProxy [reporting onPayloadReceived]: client="
            << GetClientId() << "; endpoint_id=" << endpoint_id
            << " ; payload {id:" << payload.GetId()
            << ", type:" << payload.GetType() << "}";
  listener->payload_cb(endpoint_id, std::move(payload));
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
  std::shared_ptr<const PayloadListener> listener;
  {
    absl::MutexLock lock(mutex_);
    if (!IsConnectedToEndpointLocked(endpoint_id)) {
      return;
    }
    const Connection* item = LookupConnection(endpoint_id);
    if (item == nullptr || item->payload_listener == nullptr) {
      return;
    }
    listener = item->payload_listener;
  }

  listener->payload_progress_cb(endpoint_id, info);
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

void ClientProxy::RemoveAllEndpointsLocked(
    absl::flat_hash_map<std::string, Connection>& removed_connections,
    absl::flat_hash_map<std::string, std::shared_ptr<CancellationFlag>>&
        removed_flags) {
  const bool had_connections = !connections_.empty();
  // Note: we may want to notify the client of onDisconnected() for each
  // endpoint, in the case when this is called from stopAllEndpoints(). For now,
  // just remove without notifying.
  removed_connections = std::move(connections_);
  connections_.clear();
  removed_flags = std::move(cancellation_flags_);
  cancellation_flags_.clear();

  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging) ||
      had_connections) {
    OnSessionCompleteLocked();
  }
}

void ClientProxy::ResetLocalEndpointId() {
  if (HasOngoingConnection()) {
    return;
  }
  if (!local_endpoint_id_.empty()) {
    last_local_endpoint_id_ = local_endpoint_id_;
    local_endpoint_id_.clear();
  }
}

void ClientProxy::OnSessionCompleteLocked() {
  const bool is_session_idle =
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kFixClientSessionLogging)
          ? (connections_.empty() && !IsAdvertisingLocked() &&
             !IsDiscoveringLocked() &&
             !IsListeningForIncomingConnectionsLocked())
          : (connections_.empty() && !IsAdvertisingLocked());
  if (is_session_idle) {
    if (!local_endpoint_id_.empty()) {
      last_local_endpoint_id_ = local_endpoint_id_;
    }
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
  absl::MutexLock lock(mutex_);
  return advertising_options_;
}

DiscoveryOptions ClientProxy::GetDiscoveryOptions() const {
  absl::MutexLock lock(mutex_);
  return discovery_options_;
}

v3::ConnectionListeningOptions ClientProxy::GetListeningOptions() const {
  absl::MutexLock lock(mutex_);
  return listening_options_;
}

void ClientProxy::EnterStableEndpointIdMode() {
  absl::MutexLock lock(mutex_);
  VLOG(1) << "ClientProxy [EnterStableEndpointIdMode]: client="
          << GetClientId();

  stable_endpoint_id_mode_ = true;
  if (!IsAdvertisingLocked() && !IsDiscoveringLocked() &&
      !HasOngoingConnection()) {
    ResetLocalEndpointId();
  }
}

void ClientProxy::ExitStableEndpointIdMode() {
  std::vector<std::unique_ptr<CancelableAlarm>> alarms_to_cancel;
  {
    absl::MutexLock lock(mutex_);
    ExitStableEndpointIdModeLocked(alarms_to_cancel);
  }
  CancelAlarms(std::move(alarms_to_cancel));
}

void ClientProxy::ExitStableEndpointIdModeLocked(
    std::vector<std::unique_ptr<CancelableAlarm>>& alarms_to_cancel) {
  VLOG(1) << "ClientProxy [ExitStableEndpointIdMode]: client=" << GetClientId();

  stable_endpoint_id_mode_ = false;
  ResetLocalEndpointId();
  std::unique_ptr<CancelableAlarm> alarm =
      ScheduleClearCachedEndpointIdAlarmLocked();
  if (alarm != nullptr) {
    alarms_to_cancel.push_back(std::move(alarm));
  }
}

std::unique_ptr<CancelableAlarm>
ClientProxy::ScheduleClearCachedEndpointIdAlarmLocked() {
  std::unique_ptr<CancelableAlarm> old_alarm =
      CancelClearCachedEndpointIdAlarmLocked();

  if (cached_endpoint_id_.empty()) {
    VLOG(1) << "ClientProxy [There is no cached local high power "
               "advertising endpoint Id]: client="
            << GetClientId();
    return old_alarm;
  }

  if (IsAdvertisingLocked() || IsDiscoveringLocked() ||
      HasOngoingConnection()) {
    VLOG(1) << "ClientProxy [Handle clearing cached endpoint ID "
               "during disconnection]: client="
            << GetClientId();
    return old_alarm;
  }

  // Schedule to clear cache high visibility mode advertisement endpoint id in
  // 30s.
  LOG(INFO) << "ClientProxy [High Visibility Mode Adv, Schedule to "
               "Clear Cache EndpointId]: client="
            << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
  const uint64_t generation = cached_endpoint_id_alarm_generation_;
  cached_endpoint_id_alarm_ = std::make_unique<CancelableAlarm>(
      "clear_high_power_endpoint_id_cache",
      [this, generation]() { ClearCachedLocalEndpointIdForAlarm(generation); },
      kHighPowerAdvertisementEndpointIdCacheTimeout, &single_thread_executor_);
  return old_alarm;
}

std::unique_ptr<CancelableAlarm>
ClientProxy::CancelClearCachedEndpointIdAlarmLocked() {
  ++cached_endpoint_id_alarm_generation_;
  return std::move(cached_endpoint_id_alarm_);
}

void ClientProxy::ClearCachedLocalEndpointIdForAlarm(uint64_t generation) {
  absl::MutexLock lock(mutex_);
  if (generation != cached_endpoint_id_alarm_generation_) {
    return;
  }
  LOG(INFO) << "ClientProxy [Cleared cached local endpoint Id.]: client="
            << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
  cached_endpoint_id_.clear();
}

void ClientProxy::ClearCachedLocalEndpointId() {
  absl::MutexLock lock(mutex_);
  ++cached_endpoint_id_alarm_generation_;
  LOG(INFO) << "ClientProxy [Cleared cached local endpoint Id.]: client="
            << GetClientId() << "; cached_endpoint_id_=" << cached_endpoint_id_;
  cached_endpoint_id_.clear();
}

bool ClientProxy::GetWebRtcNonCellular() const {
  absl::MutexLock lock(mutex_);
  return webrtc_non_cellular_;
}

void ClientProxy::SetWebRtcNonCellular(bool webrtc_non_cellular) {
  absl::MutexLock lock(mutex_);
  VLOG(1) << "ClientProxy: client=" << GetClientId()
          << (webrtc_non_cellular ? " disallow" : " allow")
          << " to use mobile data.";
  webrtc_non_cellular_ = webrtc_non_cellular;
}

uint8_t ClientProxy::GetDctDedup() const {
  absl::MutexLock lock(mutex_);
  return dct_dedup_;
}

void ClientProxy::UpdateDctDeviceName(absl::string_view device_name) {
  absl::MutexLock lock(mutex_);
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
  absl::MutexLock lock(mutex_);
  const Connection* item = LookupConnection(endpoint_id);
  if (item != nullptr) {
    return item->connection_options.connection_info.medium_role;
  }
  return std::nullopt;
}

location::nearby::connections::MediumRole ClientProxy::GetLocalMediumRole(
    const ClientProxy::MediumsAvailability& mediums_availability) const {
  location::nearby::connections::MediumRole medium_role;
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableDynamicRoleSwitch)) {
    return medium_role;
  }

  absl::MutexLock lock(mutex_);
  if (local_os_info_.type() == OsInfo::APPLE) {
    medium_role.set_support_awdl_publisher(true);
    medium_role.set_support_awdl_subscriber(true);
    // Apple always supports wifi hotspot client role since they can always
    // join a hotspot.
    medium_role.set_support_wifi_hotspot_client(true);
    // Apple supports the Wi-Fi Aware R4 subscriber role only; it never takes
    // the publisher role (see BwuManager::NeedToSwitchRole). Advertising this
    // lets the peer's CanHost() check decide on the right capability instead of
    // falling back to the hotspot client bit.
    medium_role.set_support_wifi_aware_subscriber(
        mediums_availability.is_wifi_aware_available);
    return medium_role;
  }

  const bool is_using_p2p = IsUsingP2pMediumLocked();
  medium_role.set_support_wifi_direct_group_owner(
      mediums_availability.is_wifi_direct_go_available && !is_using_p2p);
  medium_role.set_support_wifi_direct_group_client(
      mediums_availability.is_wifi_direct_gc_available);
  medium_role.set_support_wifi_hotspot_host(
      mediums_availability.is_wifi_hotspot_ap_available && !is_using_p2p);
  medium_role.set_support_wifi_hotspot_client(
      mediums_availability.is_wifi_hotspot_client_available);
  medium_role.set_support_wifi_aware_publisher(
      mediums_availability.is_wifi_aware_available && !is_using_p2p);
  medium_role.set_support_wifi_aware_subscriber(
      mediums_availability.is_wifi_aware_available && !is_using_p2p);
  LOG(INFO) << "medium_role: " << medium_role.DebugString();
  return medium_role;
}

bool ClientProxy::IsUsingP2pMedium() const {
  absl::MutexLock lock(mutex_);
  return IsUsingP2pMediumLocked();
}

std::optional<std::string> ClientProxy::GetEndpointIdForDct() const {
  if (dct_endpoint_id_.empty()) {
    return std::nullopt;
  }

  return dct_endpoint_id_;
}

#ifdef NEARBY_CHROMIUM
void ClientProxy::InitializePreferencesManager() {
  // This method is not currently used by Chromium.
  NOTREACHED();
}
#else
void ClientProxy::InitializePreferencesManager() {
  LOG(INFO) << "ClientProxy [InitializePreferencesManager]: client="
            << GetClientId();
  std::unique_ptr<nearby::api::DeviceInfo> device_info_ =
      nearby::api::ImplementationPlatform::CreateDeviceInfo();

  FilePath preferences_path =
      device_info_->GetLocalAppDataPath(FilePath(kPreferencesFilePath));

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
#endif

void ClientProxy::SaveClientInfoToPreferences() {
  absl::MutexLock lock(mutex_);
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

std::string ClientProxy::Dump() {
  absl::MutexLock lock(mutex_);
  std::stringstream sstream;
  sstream << "Nearby Connections State" << std::endl;
  sstream << "  Client ID: " << GetClientId() << std::endl;
  sstream << "  Local Endpoint ID: " << GetLocalEndpointIdLocked() << std::endl;
  sstream << std::boolalpha;
  sstream << "  Is Advertising: " << IsAdvertisingLocked() << std::endl;
  sstream << "  Is Discovering: " << IsDiscoveringLocked() << std::endl;
  sstream << std::noboolalpha;
  sstream << "  Advertising Service ID: " << advertising_info_.service_id
          << std::endl;
  sstream << "  Discovery Service ID: " << discovery_info_.service_id
          << std::endl;
  sstream << "  Connections: " << std::endl;
  for (auto it = connections_.begin(); it != connections_.end(); ++it) {
    // TODO(deling): write Connection.ToString()
    sstream << "    " << it->first << " :(connection token) "
            << it->second.connection_token << ", (remote os type) "
            << (it->second.os_info.has_value()
                    ? location::nearby::connections::OsInfo::OsType_Name(
                          it->second.os_info->type())
                    : "unknown")
            << ", (remote device name) " << it->second.device_name << std::endl;
  }

  sstream << "  Discovered endpoint IDs: " << std::endl;
  for (auto it = discovered_endpoint_ids_.begin();
       it != discovered_endpoint_ids_.end(); ++it) {
    sstream << "    " << *it << std::endl;
  }

  return sstream.str();
}

}  // namespace nearby::connections
