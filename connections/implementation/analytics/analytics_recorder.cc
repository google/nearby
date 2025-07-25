// Copyright 2022-2023 Google LLC
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

#include "connections/implementation/analytics/analytics_recorder.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/payload_type.h"
#include "connections/strategy.h"
#include "internal/analytics/event_logger.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace nearby {
namespace analytics {

namespace {
// const char kVersion_1_0_0[] = "v1.0.0";
const char kVersion[] = "v1.5.0";
constexpr absl::string_view kOnStartClientSession = "OnStartClientSession";
const absl::Duration kConnectionTokenMaxLife = absl::Hours(24);

using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::proto::connections::ACCEPTED;
using ::location::nearby::proto::connections::ADVERTISER;
using ::location::nearby::proto::connections::BandwidthUpgradeErrorStage;
using ::location::nearby::proto::connections::BandwidthUpgradeResult;
using ::location::nearby::proto::connections::BYTES;
using ::location::nearby::proto::connections::CLIENT_SESSION;
using ::location::nearby::proto::connections::CONNECTION_CLOSED;
using ::location::nearby::proto::connections::ConnectionAttemptDirection;
using ::location::nearby::proto::connections::ConnectionAttemptResult;
using ::location::nearby::proto::connections::ConnectionAttemptType;
using ::location::nearby::proto::connections::ConnectionBand;
using ::location::nearby::proto::connections::ConnectionRequestResponse;
using ::location::nearby::proto::connections::ConnectionsStrategy;
using ::location::nearby::proto::connections::ConnectionTechnology;
using ::location::nearby::proto::connections::DisconnectionReason;
using ::location::nearby::proto::connections::DISCOVERER;
using ::location::nearby::proto::connections::ERROR_CODE;
using ::location::nearby::proto::connections::EventType;
using ::location::nearby::proto::connections::FILE;
using ::location::nearby::proto::connections::IGNORED;
using ::location::nearby::proto::connections::INCOMING;
using ::location::nearby::proto::connections::INITIAL;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::MOVED_TO_NEW_MEDIUM;
using ::location::nearby::proto::connections::NOT_SENT;
using ::location::nearby::proto::connections::OperationResultCategory;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::OUTGOING;
using ::location::nearby::proto::connections::P2P_CLUSTER;
using ::location::nearby::proto::connections::P2P_POINT_TO_POINT;
using ::location::nearby::proto::connections::P2P_STAR;
using ::location::nearby::proto::connections::PayloadStatus;
using ::location::nearby::proto::connections::PayloadType;
using ::location::nearby::proto::connections::REJECTED;
using ::location::nearby::proto::connections::RESULT_SUCCESS;
using ::location::nearby::proto::connections::SessionRole;
using ::location::nearby::proto::connections::START_CLIENT_SESSION;
using ::location::nearby::proto::connections::START_STRATEGY_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_STRATEGY_SESSION;
using ::location::nearby::proto::connections::StopAdvertisingReason;
using ::location::nearby::proto::connections::StopDiscoveringReason;
using ::location::nearby::proto::connections::STREAM;
using ::location::nearby::proto::connections::UNFINISHED;
using ::location::nearby::proto::connections::UNFINISHED_ERROR;
using ::location::nearby::proto::connections::UNKNOWN_MEDIUM;
using ::location::nearby::proto::connections::UNKNOWN_PAYLOAD_TYPE;
using ::location::nearby::proto::connections::UNKNOWN_STRATEGY;
using ::location::nearby::proto::connections::UPGRADE_RESULT_SUCCESS;
using ::location::nearby::proto::connections::UPGRADE_SUCCESS;
using ::location::nearby::proto::connections::UPGRADE_UNFINISHED;
using ::location::nearby::proto::connections::UPGRADED;
using ::nearby::analytics::EventLogger;
using SafeDisconnectionResult = ::location::nearby::analytics::proto::
    ConnectionsLog::EstablishedConnection::SafeDisconnectionResult;

OperationResultCategory ConvertToOperationResultCategory(
    OperationResultCode result_code) {
  if (result_code == OperationResultCode::DETAIL_SUCCESS) {
    return OperationResultCategory::CATEGORY_SUCCESS;
  }
  // TODO(b/409865630): check later if we need to add back the dct error.
  // Section of CATEGORY_DCT_ERROR, from 5000 to 5499 if (result_code
  // >= OperationResultCode::DCT_ERROR_BLE_DISABLED) {
  //  return OperationResultCategory::CATEGORY_DCT_ERROR;
  //}

  // Section of CATEGORY_NEARBY_ERROR, starting from 4500 to 4999
  if (result_code >=
      OperationResultCode::NEARBY_BLE_ADVERTISEMENT_MAPPING_TO_MAC_ERROR) {
    return OperationResultCategory::CATEGORY_NEARBY_ERROR;
  }
  // Section of CATEGORY_CONNECTIVITY_ERROR, starting from 3500 to 4499
  if (result_code >=
      OperationResultCode::CONNECTIVITY_WIFI_AWARE_ATTACH_FAILURE) {
    return OperationResultCategory::CATEGORY_CONNECTIVITY_ERROR;
  }
  // Section of CATEGORY_IO_ERROR, from 3000 to 3499
  if (result_code >= OperationResultCode::IO_FILE_OPENING_ERROR) {
    return OperationResultCategory::CATEGORY_IO_ERROR;
  }
  // Section of CATEGORY_MISCELLANEOUS, from 2500 to 2999
  if (result_code >=
      OperationResultCode::MISCELLEANEOUS_BLUETOOTH_MAC_ADDRESS_NULL) {
    return OperationResultCategory::CATEGORY_MISCELLANEOUS;
  }
  // Section of CATEGORY_CLIENT_ERROR, from 2000 to 2499
  if (result_code >=
      OperationResultCode::
          CLIENT_WIFI_DIRECT_ALREADY_HOSTING_DIRECT_GROUP_FOR_THIS_CLIENT) {
    return OperationResultCategory::CATEGORY_CLIENT_ERROR;
  }
  // Section of CATEGORY_MEDIUM_UNAVAILABLE, from 1500 to 1999
  if (result_code >= OperationResultCode::
                         MEDIUM_UNAVAILABLE_WIFI_AWARE_RESOURCE_NOT_AVAILABLE) {
    return OperationResultCategory::CATEGORY_MEDIUM_UNAVAILABLE;
  }
  // Section of CATEGORY_DEVICE_STATE_ERROR, from 1000 to 1499
  if (result_code >=
      OperationResultCode::DEVICE_STATE_ERROR_UNFINISHED_UPGRADE_ATTEMPTS) {
    return OperationResultCategory::CATEGORY_DEVICE_STATE_ERROR;
  }
  // Section of CATEGORY_CLIENT_CANCELLATION, from 500 to 999
  if (result_code >=
      OperationResultCode::CLIENT_CANCELLATION_REMOTE_IN_CANCELED_STATE) {
    return OperationResultCategory::CATEGORY_CLIENT_CANCELLATION;
  }
  // Clarify other non success cases as unknown
  return OperationResultCategory::CATEGORY_UNKNOWN;
}
}  // namespace

AnalyticsRecorder::AnalyticsRecorder(EventLogger *event_logger)
    : event_logger_(event_logger) {
  LOG(INFO) << "Start AnalyticsRecorder ctor event_logger_=" << event_logger_;
  LogStartSession();
}

AnalyticsRecorder::AnalyticsRecorder(EventLogger *event_logger,
                                     bool no_record_time_millis)
    : event_logger_(event_logger),
      no_record_time_millis_(no_record_time_millis) {
  LOG(INFO) << "Start AnalyticsRecorder ctor event_logger_=" << event_logger_;
  LogStartSession();
}

AnalyticsRecorder::~AnalyticsRecorder() = default;

bool AnalyticsRecorder::IsSessionLogged() {
  MutexLock lock(&mutex_);
  return session_was_logged_;
}

int AnalyticsRecorder::GetLatestUpdateIndexLocked(
    const std::vector<ConnectionsLog::OperationResultWithMedium> &list) {
  int latest_update_index = 0;
  for (const auto &operation_result_with_medium : list) {
    if (operation_result_with_medium.update_index() > latest_update_index) {
      latest_update_index = operation_result_with_medium.update_index();
    }
  }
  return latest_update_index;
}

void AnalyticsRecorder::OnStartAdvertising(
    connections::Strategy strategy, const std::vector<Medium> &mediums,
    AdvertisingMetadataParams *advertising_metadata_params) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStartAdvertising")) {
    return;
  }
  if (!strategy.IsValid()) {
    LOG(INFO) << "AnalyticsRecorder OnStartAdvertising with unknown "
                 "strategy, bail out.";
    return;
  }
  // Initialize/update a StrategySession.
  UpdateStrategySessionLocked(strategy, ADVERTISER);

  // Initialize and set a AdvertisingPhase.
  started_advertising_phase_time_ = SystemClock::ElapsedRealtime();
  current_advertising_phase_ =
      std::make_unique<ConnectionsLog::AdvertisingPhase>();
  absl::c_copy(mediums, RepeatedFieldBackInserter(
                            current_advertising_phase_->mutable_medium()));
  // Set a AdvertisingMetadata.
  AdvertisingMetadataParams default_params = {};
  if (advertising_metadata_params == nullptr) {
    advertising_metadata_params = &default_params;
  }
  if (!advertising_metadata_params->operation_result_with_mediums.empty()) {
    absl::c_copy(advertising_metadata_params->operation_result_with_mediums,
                 RepeatedFieldBackInserter(
                     current_advertising_phase_->mutable_adv_dis_result()));
  }
  auto *advertising_metadata =
      current_advertising_phase_->mutable_advertising_metadata();
  advertising_metadata->set_supports_extended_ble_advertisements(
      advertising_metadata_params->is_extended_advertisement_supported);
  advertising_metadata->set_connected_ap_frequency(
      advertising_metadata_params->connected_ap_frequency);
  advertising_metadata->set_supports_nfc_technology(
      advertising_metadata_params->is_nfc_available);
}

void AnalyticsRecorder::OnStopAdvertising() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStopAdvertising")) {
    return;
  }
  RecordAdvertisingPhaseDurationAndReasonLocked(/* on_stop= */ true);
}

int AnalyticsRecorder::GetNextAdvertisingUpdateIndex() {
  MutexLock lock(&mutex_);

  if (current_advertising_phase_ == nullptr) {
    return 0;
  }
  return GetLatestUpdateIndexLocked(
             std::vector<ConnectionsLog::OperationResultWithMedium>(
                 current_advertising_phase_->adv_dis_result().begin(),
                 current_advertising_phase_->adv_dis_result().end())) +
         1;
}

void AnalyticsRecorder::OnStartDiscovery(
    connections::Strategy strategy, const std::vector<Medium> &mediums,
    DiscoveryMetadataParams *discovery_metadata_params) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStartDiscovery")) {
    return;
  }
  if (!strategy.IsValid()) {
    LOG(INFO) << "AnalyticsRecorder OnStartDiscovery unknown "
                 "strategy enter, bail out.";
    return;
  }

  // Initialize/update a StrategySession.
  UpdateStrategySessionLocked(strategy, DISCOVERER);

  // Initialize and set a DiscoveryPhase.
  started_discovery_phase_time_ = SystemClock::ElapsedRealtime();
  current_discovery_phase_ = std::make_unique<ConnectionsLog::DiscoveryPhase>();
  absl::c_copy(mediums, RepeatedFieldBackInserter(
                            current_discovery_phase_->mutable_medium()));
  // Set a DiscoveryMetadata.
  DiscoveryMetadataParams default_params = {};
  if (discovery_metadata_params == nullptr) {
    discovery_metadata_params = &default_params;
  }
  if (!discovery_metadata_params->operation_result_with_mediums.empty()) {
    absl::c_copy(discovery_metadata_params->operation_result_with_mediums,
                 RepeatedFieldBackInserter(
                     current_discovery_phase_->mutable_adv_dis_result()));
  }
  auto *discovery_metadata =
      current_discovery_phase_->mutable_discovery_metadata();
  discovery_metadata->set_supports_extended_ble_advertisements(
      discovery_metadata_params->is_extended_advertisement_supported);
  discovery_metadata->set_connected_ap_frequency(
      discovery_metadata_params->connected_ap_frequency);
  discovery_metadata->set_supports_nfc_technology(
      discovery_metadata_params->is_nfc_available);
}

void AnalyticsRecorder::OnStopDiscovery() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStopDiscovery")) {
    return;
  }
  RecordDiscoveryPhaseDurationAndReasonLocked(/*on_stop=*/true);
}

int AnalyticsRecorder::GetNextDiscoveryUpdateIndex() {
  MutexLock lock(&mutex_);
  if (current_discovery_phase_ == nullptr) {
    return 0;
  }
  return GetLatestUpdateIndexLocked(
             std::vector<ConnectionsLog::OperationResultWithMedium>(
                 current_discovery_phase_->adv_dis_result().begin(),
                 current_discovery_phase_->adv_dis_result().end())) +
         1;
}

void AnalyticsRecorder::OnStartedIncomingConnectionListening(
    connections::Strategy strategy) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStartedIncomingConnectionListening")) {
    return;
  }
  UpdateStrategySessionLocked(strategy, ADVERTISER);
  if (started_advertising_phase_time_ == absl::InfinitePast()) {
    started_advertising_phase_time_ = SystemClock::ElapsedRealtime();
  }
}

void AnalyticsRecorder::OnStoppedIncomingConnectionListening() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStoppedIncomingConnectionListening")) {
    return;
  }
  RecordAdvertisingPhaseDurationAndReasonLocked(/* on_stop= */ false);
}

void AnalyticsRecorder::OnEndpointFound(Medium medium) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnEndpointFound")) {
    return;
  }
  if (current_discovery_phase_ == nullptr) {
    LOG(INFO) << "Unable to record discovered endpoint due to null "
                 "current_discovery_phase_";
    return;
  }
  ConnectionsLog::DiscoveredEndpoint *discovered_endpoint =
      current_discovery_phase_->add_discovered_endpoint();
  discovered_endpoint->set_medium(medium);
  if (!no_record_time_millis_) {
    discovered_endpoint->set_latency_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_discovery_phase_time_));
  }
}

void AnalyticsRecorder::OnRequestConnection(
    const connections::Strategy &strategy, const std::string &endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("onRequestConnection")) {
    return;
  }

  UpdateStrategySessionLocked(strategy, DISCOVERER);
  if (started_discovery_phase_time_ == absl::InfinitePast()) {
    started_discovery_phase_time_ = SystemClock::ElapsedRealtime();
  }
}

void AnalyticsRecorder::OnConnectionRequestReceived(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnConnectionRequestReceived")) {
    return;
  }
  absl::Time current_time = SystemClock::ElapsedRealtime();
  auto connection_request =
      std::make_unique<ConnectionsLog::ConnectionRequest>();
  if (!no_record_time_millis_) {
    connection_request->set_duration_millis(absl::ToUnixMillis(current_time));
    connection_request->set_request_delay_millis(absl::ToInt64Milliseconds(
        current_time - started_advertising_phase_time_));
  }
  incoming_connection_requests_.insert(
      {remote_endpoint_id, std::move(connection_request)});
}

void AnalyticsRecorder::OnConnectionRequestSent(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnConnectionRequestSent")) {
    return;
  }
  absl::Time current_time = SystemClock::ElapsedRealtime();
  auto connection_request =
      std::make_unique<ConnectionsLog::ConnectionRequest>();
  if (!no_record_time_millis_) {
    connection_request->set_duration_millis(absl::ToUnixMillis(current_time));
    connection_request->set_request_delay_millis(absl::ToInt64Milliseconds(
        current_time - started_discovery_phase_time_));
  }
  outgoing_connection_requests_.insert(
      {remote_endpoint_id, std::move(connection_request)});
}

void AnalyticsRecorder::OnRemoteEndpointAccepted(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnRemoteEndpointAccepted")) {
    return;
  }
  RemoteEndpointRespondedLocked(remote_endpoint_id, ACCEPTED);
}

void AnalyticsRecorder::OnLocalEndpointAccepted(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnLocalEndpointAccepted")) {
    return;
  }
  LocalEndpointRespondedLocked(remote_endpoint_id, ACCEPTED);
}

void AnalyticsRecorder::OnRemoteEndpointRejected(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnRemoteEndpointRejected")) {
    return;
  }
  RemoteEndpointRespondedLocked(remote_endpoint_id, REJECTED);
}

void AnalyticsRecorder::OnLocalEndpointRejected(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnLocalEndpointRejected")) {
    return;
  }
  LocalEndpointRespondedLocked(remote_endpoint_id, REJECTED);
}

void AnalyticsRecorder::OnIncomingConnectionAttempt(
    ConnectionAttemptType type, Medium medium, ConnectionAttemptResult result,
    absl::Duration duration, const std::string &connection_token,
    ConnectionAttemptMetadataParams *connection_attempt_metadata_params) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnIncomingConnectionAttempt")) {
    return;
  }
  if (current_strategy_session_ == nullptr) {
    LOG(INFO) << "Unable to record incoming connection attempt due to "
                 "null current_strategy_session_";
    return;
  }

  ConnectionAttemptMetadataParams default_params = {};
  if (connection_attempt_metadata_params == nullptr) {
    connection_attempt_metadata_params = &default_params;
  }
  OnIncomingConnectionAttemptLocked(type, medium, result, duration,
                                    connection_token,
                                    connection_attempt_metadata_params);
}

void AnalyticsRecorder::OnIncomingConnectionAttemptLocked(
    location::nearby::proto::connections::ConnectionAttemptType type,
    location::nearby::proto::connections::Medium medium,
    location::nearby::proto::connections::ConnectionAttemptResult result,
    absl::Duration duration, const std::string &connection_token,
    ConnectionAttemptMetadataParams *connection_attempt_metadata_params) {
  auto *connection_attempt =
      current_strategy_session_->add_connection_attempt();
  if (!no_record_time_millis_) {
    connection_attempt->set_duration_millis(
        absl::ToInt64Milliseconds(duration));
  }
  connection_attempt->set_type(type);
  connection_attempt->set_direction(INCOMING);
  connection_attempt->set_medium(medium);
  connection_attempt->set_attempt_result(result);
  connection_attempt->set_connection_token(connection_token);

  auto *connection_attempt_metadata =
      connection_attempt->mutable_connection_attempt_metadata();
  connection_attempt_metadata->set_technology(
      connection_attempt_metadata_params->technology);
  connection_attempt_metadata->set_band(
      connection_attempt_metadata_params->band);
  connection_attempt_metadata->set_frequency(
      connection_attempt_metadata_params->frequency);
  connection_attempt_metadata->set_network_operator(
      connection_attempt_metadata_params->network_operator);
  connection_attempt_metadata->set_country_code(
      connection_attempt_metadata_params->country_code);
  connection_attempt_metadata->set_frequency(
      connection_attempt_metadata_params->frequency);
  connection_attempt_metadata->set_is_tdls_used(
      connection_attempt_metadata_params->is_tdls_used);
  connection_attempt_metadata->set_wifi_hotspot_status(
      connection_attempt_metadata_params->wifi_hotspot_enabled);
  connection_attempt_metadata->set_try_counts(
      connection_attempt_metadata_params->try_count);
  connection_attempt_metadata->set_max_tx_speed(
      connection_attempt_metadata_params->max_wifi_tx_speed);
  connection_attempt_metadata->set_max_rx_speed(
      connection_attempt_metadata_params->max_wifi_rx_speed);
  connection_attempt_metadata->set_wifi_channel_width(
      connection_attempt_metadata_params->channel_width);

  auto operation_result_proto =
      std::make_unique<ConnectionsLog::OperationResult>();
  operation_result_proto->set_result_code(
      connection_attempt_metadata_params->operation_result_code);
  operation_result_proto->set_result_category(ConvertToOperationResultCategory(
      connection_attempt_metadata_params->operation_result_code));
  connection_attempt->set_allocated_operation_result(
      operation_result_proto.release());
}

void AnalyticsRecorder::OnOutgoingConnectionAttempt(
    const std::string &remote_endpoint_id, ConnectionAttemptType type,
    Medium medium, ConnectionAttemptResult result, absl::Duration duration,
    const std::string &connection_token,
    ConnectionAttemptMetadataParams *connection_attempt_metadata_params) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnOutgoingConnectionAttempt")) {
    return;
  }
  if (current_strategy_session_ == nullptr) {
    LOG(INFO) << "Unable to record outgoing connection attempt due to "
                 "null current_strategy_session_";
    return;
  }

  ConnectionAttemptMetadataParams default_params = {};
  if (connection_attempt_metadata_params == nullptr) {
    connection_attempt_metadata_params = &default_params;
  }

  // For the case of transfer a big file and the upgrades always failure, then
  // there will have repeating upgrade attempt and cause many same attempt value
  // be log. So add a method to skip.
  if (ConnectionAttemptResultCodeExistedLocked(
          medium, OUTGOING, connection_token, type,
          connection_attempt_metadata_params->operation_result_code)) {
    return;
  }

  OnOutgoingConnectionAttemptLocked(remote_endpoint_id, type, medium, result,
                                    duration, connection_token,
                                    connection_attempt_metadata_params);
}

void AnalyticsRecorder::OnOutgoingConnectionAttemptLocked(
    const std::string &remote_endpoint_id, ConnectionAttemptType type,
    Medium medium, ConnectionAttemptResult result, absl::Duration duration,
    const std::string &connection_token,
    ConnectionAttemptMetadataParams *connection_attempt_metadata_params) {
  auto *connection_attempt =
      current_strategy_session_->add_connection_attempt();
  if (!no_record_time_millis_) {
    connection_attempt->set_duration_millis(
        absl::ToInt64Milliseconds(duration));
  }
  connection_attempt->set_type(type);
  connection_attempt->set_direction(OUTGOING);
  connection_attempt->set_medium(medium);
  connection_attempt->set_attempt_result(result);
  connection_attempt->set_connection_token(connection_token);

  auto *connection_attempt_metadata =
      connection_attempt->mutable_connection_attempt_metadata();
  connection_attempt_metadata->set_technology(
      connection_attempt_metadata_params->technology);
  connection_attempt_metadata->set_band(
      connection_attempt_metadata_params->band);
  connection_attempt_metadata->set_frequency(
      connection_attempt_metadata_params->frequency);
  connection_attempt_metadata->set_network_operator(
      connection_attempt_metadata_params->network_operator);
  connection_attempt_metadata->set_country_code(
      connection_attempt_metadata_params->country_code);
  connection_attempt_metadata->set_frequency(
      connection_attempt_metadata_params->frequency);
  connection_attempt_metadata->set_is_tdls_used(
      connection_attempt_metadata_params->is_tdls_used);
  connection_attempt_metadata->set_wifi_hotspot_status(
      connection_attempt_metadata_params->wifi_hotspot_enabled);
  connection_attempt_metadata->set_try_counts(
      connection_attempt_metadata_params->try_count);
  connection_attempt_metadata->set_max_tx_speed(
      connection_attempt_metadata_params->max_wifi_tx_speed);
  connection_attempt_metadata->set_max_rx_speed(
      connection_attempt_metadata_params->max_wifi_rx_speed);
  connection_attempt_metadata->set_wifi_channel_width(
      connection_attempt_metadata_params->channel_width);

  auto operation_result_proto =
      std::make_unique<ConnectionsLog::OperationResult>();
  operation_result_proto->set_result_code(
      connection_attempt_metadata_params->operation_result_code);
  operation_result_proto->set_result_category(ConvertToOperationResultCategory(
      connection_attempt_metadata_params->operation_result_code));
  connection_attempt->set_allocated_operation_result(
      operation_result_proto.release());

  if (type == INITIAL && result != RESULT_SUCCESS) {
    auto it = outgoing_connection_requests_.find(remote_endpoint_id);
    if (it != outgoing_connection_requests_.end()) {
      // An outgoing, initial ConnectionAttempt has a corresponding
      // ConnectionRequest that, since the ConnectionAttempt has failed, will
      // never be delivered to the advertiser.
      auto pair = outgoing_connection_requests_.extract(it);
      std::unique_ptr<ConnectionsLog::ConnectionRequest> &connection_request =
          pair.mapped();
      connection_request->set_local_response(NOT_SENT);
      connection_request->set_remote_response(NOT_SENT);
      UpdateDiscovererConnectionRequestLocked(connection_request.get());
    }
  }
}

void AnalyticsRecorder::OnConnectionEstablished(
    const std::string &endpoint_id, Medium medium,
    const std::string &connection_token) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnConnectionEstablished")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it != active_connections_.end()) {
    const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
    logical_connection->PhysicalConnectionEstablished(medium, connection_token);
  } else {
    active_connections_.insert(
        {endpoint_id, std::make_unique<LogicalConnection>(
                          medium, connection_token, no_record_time_millis_)});
  }
}

void AnalyticsRecorder::OnConnectionClosed(const std::string &endpoint_id,
                                           Medium medium,
                                           DisconnectionReason reason,
                                           SafeDisconnectionResult result) {
  MutexLock lock(&mutex_);
  LOG(INFO) << __func__
            << ": OnConnectionClosed is called with endpoint_id:" << endpoint_id
            << ", medium:" << Medium_Name(medium)
            << ", reason:" << DisconnectionReason_Name(reason)
            << ", result:" << result;

  if (!CanRecordAnalyticsLocked("OnConnectionClosed")) {
    return;
  }

  if (current_strategy_session_ == nullptr) {
    VLOG(1) << "AnalyticsRecorder CanRecordAnalytics Unexpected call "
            << __func__ << " since current_strategy_session_ is required.";
    return;
  }

  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }
  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->PhysicalConnectionClosed(medium, reason, result);
  if (reason != UPGRADED) {
    // Unless this is an upgraded connection, remove this from our active
    // connections. Any future communication with an endpoint will need to be
    // re-established with a new ConnectionRequest.
    auto pair = active_connections_.extract(it);
    std::unique_ptr<LogicalConnection> &logical_connection = pair.mapped();

    absl::c_copy(
        logical_connection->GetEstablisedConnections(),
        RepeatedFieldBackInserter(
            current_strategy_session_->mutable_established_connection()));
  }
}

void AnalyticsRecorder::OnIncomingPayloadStarted(
    const std::string &endpoint_id, std::int64_t payload_id,
    connections::PayloadType type, std::int64_t total_size_bytes) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnIncomingPayloadStarted")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }
  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->IncomingPayloadStarted(
      payload_id, PayloadTypeToProtoPayloadType(type), total_size_bytes);
}

void AnalyticsRecorder::OnPayloadChunkReceived(const std::string &endpoint_id,
                                               std::int64_t payload_id,
                                               std::int64_t chunk_size_bytes) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnPayloadChunkReceived")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }
  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->ChunkReceived(payload_id, chunk_size_bytes);
}

void AnalyticsRecorder::OnIncomingPayloadDone(
    const std::string &endpoint_id, std::int64_t payload_id,
    PayloadStatus status, OperationResultCode operation_result_code) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnIncomingPayloadDone")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }
  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->IncomingPayloadDone(payload_id, status,
                                          operation_result_code);
}

void AnalyticsRecorder::OnOutgoingPayloadStarted(
    const std::vector<std::string> &endpoint_ids, std::int64_t payload_id,
    connections::PayloadType type, std::int64_t total_size_bytes) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnOutgoingPayloadStarted")) {
    return;
  }
  for (const auto &endpoint_id : endpoint_ids) {
    auto it = active_connections_.find(endpoint_id);
    if (it == active_connections_.end()) {
      continue;
    }
    const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
    logical_connection->OutgoingPayloadStarted(
        payload_id, PayloadTypeToProtoPayloadType(type), total_size_bytes);
  }
}

void AnalyticsRecorder::OnPayloadChunkSent(const std::string &endpoint_id,
                                           std::int64_t payload_id,
                                           std::int64_t chunk_size_bytes) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnPayloadChunkSent")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }
  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->ChunkSent(payload_id, chunk_size_bytes);
}

void AnalyticsRecorder::OnOutgoingPayloadDone(
    const std::string &endpoint_id, std::int64_t payload_id,
    PayloadStatus status, OperationResultCode operation_result_code) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnOutgoingPayloadDone")) {
    return;
  }
  auto it = active_connections_.find(endpoint_id);
  if (it == active_connections_.end()) {
    return;
  }

  const std::unique_ptr<LogicalConnection> &logical_connection = it->second;
  logical_connection->OutgoingPayloadDone(payload_id, status,
                                          operation_result_code);
}

void AnalyticsRecorder::OnBandwidthUpgradeStarted(
    const std::string &endpoint_id, Medium from_medium, Medium to_medium,
    ConnectionAttemptDirection direction, const std::string &connection_token) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnBandwidthUpgradeStarted")) {
    return;
  }
  auto bandwidth_upgrade_attempt =
      std::make_unique<ConnectionsLog::BandwidthUpgradeAttempt>();
  if (!no_record_time_millis_) {
    bandwidth_upgrade_attempt->set_duration_millis(
        absl::ToUnixMillis(SystemClock::ElapsedRealtime()));
  }
  bandwidth_upgrade_attempt->set_from_medium(from_medium);
  bandwidth_upgrade_attempt->set_to_medium(to_medium);
  bandwidth_upgrade_attempt->set_direction(direction);
  bandwidth_upgrade_attempt->set_connection_token(connection_token);
  bandwidth_upgrade_attempts_.insert(
      {endpoint_id, std::move(bandwidth_upgrade_attempt)});
}

void AnalyticsRecorder::OnBandwidthUpgradeError(
    const std::string &endpoint_id, BandwidthUpgradeResult result,
    BandwidthUpgradeErrorStage error_stage,
    OperationResultCode operation_result_code) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnBandwidthUpgradeError")) {
    return;
  }
  // If the same records existed, drop this one.
  if (EraseIfBandwidthUpgradeRecordExistedLocked(
          endpoint_id, result, error_stage, operation_result_code)) {
    return;
  }
  FinishUpgradeAttemptLocked(endpoint_id, result, error_stage,
                             operation_result_code);
}

void AnalyticsRecorder::OnBandwidthUpgradeSuccess(
    const std::string &endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnBandwidthUpgradeSuccess")) {
    return;
  }
  FinishUpgradeAttemptLocked(endpoint_id, UPGRADE_RESULT_SUCCESS,
                             UPGRADE_SUCCESS,
                             OperationResultCode::DETAIL_SUCCESS);
}

void AnalyticsRecorder::OnErrorCode(const ErrorCodeParams &params) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnErrorCode")) {
    return;
  }
  auto error_code = std::make_unique<ConnectionsLog::ErrorCode>();
  error_code->set_medium(params.medium);
  error_code->set_event(params.event);
  error_code->set_connection_token(params.connection_token);
  error_code->set_description(params.description);

  if (params.is_common_error) {
    error_code->set_common_error(params.common_error);
  } else {
    switch (params.event) {
      case location::nearby::errorcode::proto::START_ADVERTISING:
        error_code->set_start_advertising_error(params.start_advertising_error);
        break;
      case location::nearby::errorcode::proto::STOP_ADVERTISING:
        error_code->set_stop_advertising_error(params.stop_advertising_error);
        break;
      case location::nearby::errorcode::proto::
          START_LISTENING_INCOMING_CONNECTION:
        error_code->set_start_listening_incoming_connection_error(
            params.start_listening_incoming_connection_error);
        break;
      case location::nearby::errorcode::proto::
          STOP_LISTENING_INCOMING_CONNECTION:
        error_code->set_stop_listening_incoming_connection_error(
            params.stop_listening_incoming_connection_error);
        break;
      case location::nearby::errorcode::proto::START_DISCOVERING:
        error_code->set_start_discovering_error(params.start_discovering_error);
        break;
      case location::nearby::errorcode::proto::STOP_DISCOVERING:
        error_code->set_stop_discovering_error(params.stop_discovering_error);
        break;
      case location::nearby::errorcode::proto::CONNECT:
        error_code->set_connect_error(params.connect_error);
        break;
      case location::nearby::errorcode::proto::DISCONNECT:
        error_code->set_disconnect_error(params.disconnect_error);
        break;
      case location::nearby::errorcode::proto::UNKNOWN_EVENT:
      default:
        error_code->set_common_error(params.common_error);
        break;
    }
  }

  ConnectionsLog connections_log;
  connections_log.set_event_type(ERROR_CODE);
  connections_log.set_version(kVersion);
  connections_log.set_allocated_error_code(error_code.release());

  VLOG(1) << "AnalyticsRecorder LogErrorCode connections_log="
          << connections_log.DebugString();  // NOLINT

  event_logger_->Log(connections_log);
}

void AnalyticsRecorder::LogStartSession() {
  MutexLock lock(&mutex_);
  if (start_client_session_was_logged_) {
    LOG(WARNING) << "AnalyticsRecorder CanRecordAnalytics Unexpected call "
                 << kOnStartClientSession
                 << " after start client session has already been logged.";
    return;
  }

  session_was_logged_ = false;
  if (CanRecordAnalyticsLocked(kOnStartClientSession)) {
    client_session_ = std::make_unique<ConnectionsLog::ClientSession>();
    started_client_session_time_ = SystemClock::ElapsedRealtime();
    start_client_session_was_logged_ = true;
    LogEvent(START_CLIENT_SESSION);
  }
}

void AnalyticsRecorder::LogSession() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("LogSession")) {
    return;
  }
  FinishStrategySessionLocked();
  if (!no_record_time_millis_) {
    client_session_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_client_session_time_));
  }
  LogClientSessionLocked();
  LogEvent(STOP_CLIENT_SESSION);
  start_client_session_was_logged_ = false;
  session_was_logged_ = true;
}

std::unique_ptr<AdvertisingMetadataParams>
AnalyticsRecorder::BuildAdvertisingMetadataParams(
    bool is_extended_advertisement_supported, int connected_ap_frequency,
    bool is_nfc_available,
    const std::vector<ConnectionsLog::OperationResultWithMedium>
        &operation_result_with_mediums) {
  auto params = std::make_unique<AdvertisingMetadataParams>();
  params->is_extended_advertisement_supported =
      is_extended_advertisement_supported;
  params->connected_ap_frequency = connected_ap_frequency;
  params->is_nfc_available = is_nfc_available;
  params->operation_result_with_mediums =
      std::move(operation_result_with_mediums);
  return params;
}

std::unique_ptr<DiscoveryMetadataParams>
AnalyticsRecorder::BuildDiscoveryMetadataParams(
    bool is_extended_advertisement_supported, int connected_ap_frequency,
    bool is_nfc_available,
    const std::vector<ConnectionsLog::OperationResultWithMedium>
        &operation_result_with_mediums) {
  auto params = std::make_unique<DiscoveryMetadataParams>();
  params->is_extended_advertisement_supported =
      is_extended_advertisement_supported;
  params->connected_ap_frequency = connected_ap_frequency;
  params->is_nfc_available = is_nfc_available;
  params->operation_result_with_mediums =
      std::move(operation_result_with_mediums);
  return params;
}

std::unique_ptr<ConnectionAttemptMetadataParams>
AnalyticsRecorder::BuildConnectionAttemptMetadataParams(
    ConnectionTechnology technology, ConnectionBand band, int frequency,
    int try_count, const std::string &network_operator,
    const std::string &country_code, bool is_tdls_used,
    bool wifi_hotspot_enabled, int max_wifi_tx_speed, int max_wifi_rx_speed,
    int channel_width, OperationResultCode operation_result_code) {
  auto params = std::make_unique<ConnectionAttemptMetadataParams>();
  params->technology = technology;
  params->band = band;
  params->frequency = frequency;
  params->try_count = try_count;
  params->network_operator = network_operator;
  params->country_code = country_code;
  params->is_tdls_used = is_tdls_used;
  params->wifi_hotspot_enabled = wifi_hotspot_enabled;
  params->max_wifi_tx_speed = max_wifi_tx_speed;
  params->max_wifi_rx_speed = max_wifi_rx_speed;
  params->channel_width = channel_width;
  params->operation_result_code = operation_result_code;
  return params;
}

OperationResultCode AnalyticsRecorder::GetChannelIoErrorResultCodeFromMedium(
    Medium medium) {
  switch (medium) {
    case Medium::BLUETOOTH:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BT;
    case Medium::WIFI_HOTSPOT:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_HOTSPOT;
    case Medium::BLE:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BLE;
    case Medium::BLE_L2CAP:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BLE_L2CAP;
    case Medium::WIFI_LAN:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_LAN;
    case Medium::WIFI_AWARE:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_AWARE;
    case Medium::NFC:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_NFC;
    case Medium::WIFI_DIRECT:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_DIRECT;
    case Medium::WEB_RTC:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WEB_RTC;
    default:
      return OperationResultCode::
          CONNECTIVITY_CHANNEL_IO_ERROR_ON_UNKNOWN_MEDIUM;
  }
}

bool AnalyticsRecorder::CanRecordAnalyticsLocked(
    absl::string_view method_name) {
  VLOG(1) << "AnalyticsRecorder LogEvent " << method_name << " is calling.";
  if (event_logger_ == nullptr) {
    return false;
  }

  if (session_was_logged_) {
    VLOG(1) << "AnalyticsRecorder CanRecordAnalytics Unexpected call "
            << method_name << " after session has already been logged.";
    return false;
  }

  return true;
}

// TODO: b/391339677 - Investigate why we need to reset the resources. And
// verify in b/238375695 to see if we still meet the issue after removing the
// Reset function.
void AnalyticsRecorder::LogClientSessionLocked() {
  ConnectionsLog connections_log;
  connections_log.set_event_type(CLIENT_SESSION);
  connections_log.set_allocated_client_session(client_session_.release());
  connections_log.set_version(kVersion);

  VLOG(1) << "AnalyticsRecorder LogClientSession connections_log="
          << connections_log.DebugString();  // NOLINT

  event_logger_->Log(connections_log);
  client_session_ = nullptr;
}

void AnalyticsRecorder::LogEvent(EventType event_type) {
  ConnectionsLog connections_log;
  connections_log.set_event_type(event_type);
  connections_log.set_version(kVersion);

  VLOG(1) << "AnalyticsRecorder LogEvent connections_log="
          << connections_log.DebugString();  // NOLINT

  event_logger_->Log(connections_log);
}

void AnalyticsRecorder::UpdateStrategySessionLocked(
    connections::Strategy strategy, SessionRole role) {
  // If we're not switching strategies, just update the current StrategySession
  // with the new role.
  if (strategy == current_strategy_ && current_strategy_session_ != nullptr) {
    if (absl::c_linear_search(current_strategy_session_->role(), role)) {
      // We've already acted as this role before, so make sure we've finished
      // recording the previous round.
      switch (role) {
        case ADVERTISER:
          FinishAdvertisingPhaseLocked();
          break;
        case DISCOVERER:
          FinishDiscoveryPhaseLocked();
          break;
        default:
          break;
      }
    } else {
      current_strategy_session_->add_role(role);
    }
  } else {
    // Otherwise, we're starting a new Strategy.
    current_strategy_ = strategy;
    FinishStrategySessionLocked();
    LogEvent(START_STRATEGY_SESSION);
    current_strategy_session_ =
        std::make_unique<ConnectionsLog::StrategySession>();
    started_strategy_session_time_ = SystemClock::ElapsedRealtime();
    current_strategy_session_->set_strategy(
        StrategyToConnectionStrategy(strategy));
    current_strategy_session_->add_role(role);
  }
}

void AnalyticsRecorder::RecordAdvertisingPhaseDurationAndReasonLocked(
    bool on_stop) const {
  if (current_advertising_phase_ == nullptr) {
    LOG(INFO) << "Unable to record advertising phase duration due to "
                 "null current_advertising_phase_";
    return;
  }
  if (!current_advertising_phase_->has_duration_millis() &&
      !no_record_time_millis_) {
    current_advertising_phase_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_advertising_phase_time_));
  }
  if (!current_advertising_phase_->has_stop_reason()) {
    current_advertising_phase_->set_stop_reason(
        on_stop ? StopAdvertisingReason::CLIENT_STOP_ADVERTISING
                : StopAdvertisingReason::FINISH_SESSION_STOP_ADVERTISING);
  }
}

void AnalyticsRecorder::FinishAdvertisingPhaseLocked() {
  if (current_advertising_phase_ != nullptr) {
    for (const auto &item : incoming_connection_requests_) {
      // ConnectionRequests still pending have been ignored by the local or
      // remote (or both) endpoints.
      const std::unique_ptr<ConnectionsLog::ConnectionRequest>
          &connection_request = item.second;
      MarkConnectionRequestIgnoredLocked(connection_request.get());
      UpdateAdvertiserConnectionRequestLocked(connection_request.get());
    }
    RecordAdvertisingPhaseDurationAndReasonLocked(/* on_stop= */ false);
    if (current_strategy_session_ != nullptr) {
      *current_strategy_session_->add_advertising_phase() =
          *std::move(current_advertising_phase_);
    } else {
      LOG(INFO) << "Unable to record advertising phase due to null "
                   "current_strategy_session_";
    }
  }
  incoming_connection_requests_.clear();
}

void AnalyticsRecorder::RecordDiscoveryPhaseDurationAndReasonLocked(
    bool on_stop) const {
  if (current_discovery_phase_ == nullptr) {
    LOG(INFO) << "Unable to record discovery phase duration due to "
                 "null current_discovery_phase_";
    return;
  }
  if (!current_discovery_phase_->has_duration_millis() &&
      !no_record_time_millis_) {
    current_discovery_phase_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_discovery_phase_time_));
  }
  // If the stop reason haven't been set yet, then set it.
  if (!current_discovery_phase_->has_stop_reason()) {
    current_discovery_phase_->set_stop_reason(
        on_stop ? StopDiscoveringReason::CLIENT_STOP_DISCOVERING
                : StopDiscoveringReason::FINISH_SESSION_STOP_DISCOVERING);
  }
}

void AnalyticsRecorder::FinishDiscoveryPhaseLocked() {
  if (current_discovery_phase_ != nullptr) {
    for (const auto &item : outgoing_connection_requests_) {
      // ConnectionRequests still pending have been ignored by the local or
      // remote (or both) endpoints.
      const std::unique_ptr<ConnectionsLog::ConnectionRequest>
          &connection_request = item.second;
      MarkConnectionRequestIgnoredLocked(connection_request.get());
      UpdateDiscovererConnectionRequestLocked(connection_request.get());
    }
    RecordDiscoveryPhaseDurationAndReasonLocked(/* on_stop=*/false);
    if (current_strategy_session_ != nullptr) {
      *current_strategy_session_->add_discovery_phase() =
          *std::move(current_discovery_phase_);
    } else {
      LOG(INFO) << "Unable to record discovery phase due to null "
                   "current_strategy_session_";
    }
  }
  outgoing_connection_requests_.clear();
}

bool AnalyticsRecorder::UpdateAdvertiserConnectionRequestLocked(
    ConnectionsLog::ConnectionRequest *request) {
  if (current_advertising_phase_ == nullptr) {
    LOG(INFO) << "Unable to record advertiser connection request due to null "
                 "current_advertising_phase_";
    return false;
  }
  if (BothEndpointsRespondedLocked(request)) {
    if (!no_record_time_millis_) {
      request->set_duration_millis(
          absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
          request->duration_millis());
    }
    *current_advertising_phase_->add_received_connection_request() = *request;
    return true;
  }
  return false;
}

bool AnalyticsRecorder::UpdateDiscovererConnectionRequestLocked(
    ConnectionsLog::ConnectionRequest *request) {
  if (current_discovery_phase_ == nullptr) {
    LOG(INFO) << "Unable to record discoverer connection request due "
                 "to null current_discovery_phase_.";
    return false;
  }
  if (BothEndpointsRespondedLocked(request) ||
      request->local_response() == NOT_SENT) {
    if (!no_record_time_millis_) {
      request->set_duration_millis(
          absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
          request->duration_millis());
    }
    *current_discovery_phase_->add_sent_connection_request() = *request;
    return true;
  }
  return false;
}

bool AnalyticsRecorder::BothEndpointsRespondedLocked(
    ConnectionsLog::ConnectionRequest *request) {
  return request->has_local_response() && request->has_remote_response();
}

void AnalyticsRecorder::LocalEndpointRespondedLocked(
    const std::string &remote_endpoint_id, ConnectionRequestResponse response) {
  auto out = outgoing_connection_requests_.find(remote_endpoint_id);
  if (out != outgoing_connection_requests_.end()) {
    ConnectionsLog::ConnectionRequest *connection_request = out->second.get();
    connection_request->set_local_response(response);
    if (UpdateDiscovererConnectionRequestLocked(connection_request)) {
      outgoing_connection_requests_.erase(out);
    }
  }
  auto in = incoming_connection_requests_.find(remote_endpoint_id);
  if (in != incoming_connection_requests_.end()) {
    ConnectionsLog::ConnectionRequest *connection_request = in->second.get();
    connection_request->set_local_response(response);
    if (UpdateAdvertiserConnectionRequestLocked(connection_request)) {
      incoming_connection_requests_.erase(in);
    }
  }
}

void AnalyticsRecorder::RemoteEndpointRespondedLocked(
    const std::string &remote_endpoint_id, ConnectionRequestResponse response) {
  auto out = outgoing_connection_requests_.find(remote_endpoint_id);
  if (out != outgoing_connection_requests_.end()) {
    ConnectionsLog::ConnectionRequest *connection_request = out->second.get();
    connection_request->set_remote_response(response);
    if (UpdateDiscovererConnectionRequestLocked(connection_request)) {
      outgoing_connection_requests_.erase(out);
    }
  }
  auto in = incoming_connection_requests_.find(remote_endpoint_id);
  if (in != incoming_connection_requests_.end()) {
    ConnectionsLog::ConnectionRequest *connection_request = in->second.get();
    connection_request->set_remote_response(response);
    if (UpdateAdvertiserConnectionRequestLocked(connection_request)) {
      incoming_connection_requests_.erase(in);
    }
  }
}

void AnalyticsRecorder::MarkConnectionRequestIgnoredLocked(
    ConnectionsLog::ConnectionRequest *request) {
  if (!request->has_local_response()) {
    request->set_local_response(IGNORED);
  }
  if (!request->has_remote_response()) {
    request->set_remote_response(IGNORED);
  }
}

bool AnalyticsRecorder::ConnectionAttemptResultCodeExistedLocked(
    Medium medium, ConnectionAttemptDirection direction,
    const std::string &connection_token, ConnectionAttemptType type,
    OperationResultCode operation_result_code) {
  if (current_strategy_session_ == nullptr ||
      current_strategy_session_->connection_attempt_size() == 0) {
    return false;
  }
  for (auto &connection_attempt :
       current_strategy_session_->connection_attempt()) {
    if (connection_attempt.medium() == medium &&
        connection_attempt.direction() == direction &&
        connection_attempt.connection_token() == connection_token &&
        connection_attempt.type() == type &&
        connection_attempt.operation_result().result_code() ==
            operation_result_code) {
      return true;
    }
  }

  return false;
}

// If bandwidth upgrade always failed on the same fromMedium, toMedium, result,
// stage and result code, we'll drop the duplicate logs for preventing the waste
// of log storage space
bool AnalyticsRecorder::EraseIfBandwidthUpgradeRecordExistedLocked(
    const std::string &endpoint_id, BandwidthUpgradeResult result,
    BandwidthUpgradeErrorStage error_stage,
    OperationResultCode operation_result_code) {
  if (current_strategy_session_ == nullptr) {
    return false;
  }
  auto it = bandwidth_upgrade_attempts_.find(endpoint_id);
  if (it != bandwidth_upgrade_attempts_.end()) {
    ConnectionsLog::BandwidthUpgradeAttempt *attempt = it->second.get();
    for (auto &existing_attempt :
         current_strategy_session_->upgrade_attempt()) {
      if (attempt->from_medium() == existing_attempt.from_medium() &&
          attempt->to_medium() == existing_attempt.to_medium() &&
          result == existing_attempt.upgrade_result() &&
          error_stage == existing_attempt.error_stage() &&
          operation_result_code ==
              existing_attempt.operation_result().result_code()) {
        bandwidth_upgrade_attempts_.erase(it);
        return true;
      }
    }
  }
  return false;
}

void AnalyticsRecorder::FinishUpgradeAttemptLocked(
    const std::string &endpoint_id, BandwidthUpgradeResult result,
    BandwidthUpgradeErrorStage error_stage,
    OperationResultCode operation_result_code, bool erase_item) {
  if (current_strategy_session_ == nullptr) {
    LOG(INFO) << "Unable to record upgrade attempt due to null "
                 "current_strategy_session_";
    return;
  }
  // Add the BandwidthUpgradeAttempt in the current StrategySession.
  auto it = bandwidth_upgrade_attempts_.find(endpoint_id);
  if (it != bandwidth_upgrade_attempts_.end()) {
    ConnectionsLog::BandwidthUpgradeAttempt *attempt = it->second.get();
    if (!no_record_time_millis_) {
      attempt->set_duration_millis(
          absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
          attempt->duration_millis());
    }
    attempt->set_error_stage(error_stage);
    attempt->set_upgrade_result(result);

    auto operation_result_proto =
        std::make_unique<ConnectionsLog::OperationResult>();
    operation_result_proto->set_result_code(operation_result_code);
    operation_result_proto->set_result_category(
        ConvertToOperationResultCategory(operation_result_code));
    attempt->set_allocated_operation_result(operation_result_proto.release());
    *current_strategy_session_->add_upgrade_attempt() = *attempt;
    if (erase_item) {
      bandwidth_upgrade_attempts_.erase(it);
    }
  }
}

void AnalyticsRecorder::FinishStrategySessionLocked() {
  if (current_strategy_session_ != nullptr) {
    FinishAdvertisingPhaseLocked();
    FinishDiscoveryPhaseLocked();

    // Finish any unfinished LogicalConnections.
    for (const auto &item : active_connections_) {
      const std::unique_ptr<LogicalConnection> &logical_connection =
          item.second;
      logical_connection->CloseAllPhysicalConnections();
      absl::c_copy(
          logical_connection->GetEstablisedConnections(),
          RepeatedFieldBackInserter(
              current_strategy_session_->mutable_established_connection()));
    }
    active_connections_.clear();

    // Finish any pending upgrade attempts.
    for (const auto &item : bandwidth_upgrade_attempts_) {
      FinishUpgradeAttemptLocked(
          item.first, UNFINISHED_ERROR, UPGRADE_UNFINISHED,
          OperationResultCode::DEVICE_STATE_ERROR_UNFINISHED_UPGRADE_ATTEMPTS,
          /*erase_item=*/false);
    }
    bandwidth_upgrade_attempts_.clear();

    // Add the StrategySession in ClientSession
    if (current_strategy_session_ != nullptr) {
      if (!no_record_time_millis_) {
        current_strategy_session_->set_duration_millis(
            absl::ToInt64Milliseconds(SystemClock::ElapsedRealtime() -
                                      started_strategy_session_time_));
      }
      *client_session_->add_strategy_session() =
          *std::move(current_strategy_session_);
    }

    current_strategy_session_ = nullptr;
    current_strategy_ = connections::Strategy::kNone;
    LogEvent(STOP_STRATEGY_SESSION);
  }
}

ConnectionsStrategy AnalyticsRecorder::StrategyToConnectionStrategy(
    connections::Strategy strategy) {
  if (strategy == connections::Strategy::kP2pCluster) {
    return P2P_CLUSTER;
  }
  if (strategy == connections::Strategy::kP2pStar) {
    return P2P_STAR;
  }
  if (strategy == connections::Strategy::kP2pPointToPoint) {
    return P2P_POINT_TO_POINT;
  }
  return UNKNOWN_STRATEGY;
}

PayloadType AnalyticsRecorder::PayloadTypeToProtoPayloadType(
    connections::PayloadType type) {
  switch (type) {
    case connections::PayloadType::kBytes:
      return BYTES;
    case connections::PayloadType::kFile:
      return FILE;
    case connections::PayloadType::kStream:
      return STREAM;
    default:
      return UNKNOWN_PAYLOAD_TYPE;
  }
}

void AnalyticsRecorder::PendingPayload::AddChunk(
    std::int64_t chunk_size_bytes) {
  num_bytes_transferred_ += chunk_size_bytes;
  num_chunks_++;
}

ConnectionsLog::Payload AnalyticsRecorder::PendingPayload::GetProtoPayload(
    PayloadStatus status) {
  ConnectionsLog::Payload payload;
  if (!no_record_time_millis_) {
    payload.set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - start_time_));
  }
  payload.set_type(type_);
  payload.set_total_size_bytes(total_size_bytes_);
  payload.set_num_bytes_transferred(num_bytes_transferred_);
  payload.set_num_chunks(num_chunks_);
  payload.set_status(status);

  auto operation_result_proto =
      std::make_unique<ConnectionsLog::OperationResult>();
  operation_result_proto->set_result_code(operation_result_code_);
  operation_result_proto->set_result_category(
      ConvertToOperationResultCategory(operation_result_code_));
  payload.set_allocated_operation_result(operation_result_proto.release());

  return payload;
}

void AnalyticsRecorder::LogicalConnection::PhysicalConnectionEstablished(
    Medium medium, const std::string &connection_token) {
  if (current_medium_ != UNKNOWN_MEDIUM) {
    LOG(WARNING) << "Unexpected call to PhysicalConnectionEstablished while "
                    "AnalyticsRecorder still has an active current medium.";
  }

  auto established_connection =
      std::make_unique<ConnectionsLog::EstablishedConnection>();
  established_connection->set_medium(medium);
  if (!no_record_time_millis_) {
    established_connection->set_duration_millis(
        absl::ToUnixMillis(SystemClock::ElapsedRealtime()));
  }
  established_connection->set_connection_token(connection_token);

  auto operation_result_proto =
      std::make_unique<ConnectionsLog::OperationResult>();
  operation_result_proto->set_result_code(OperationResultCode::DETAIL_SUCCESS);
  operation_result_proto->set_result_category(
      OperationResultCategory::CATEGORY_SUCCESS);
  established_connection->set_allocated_operation_result(
      operation_result_proto.release());
  physical_connections_.insert({medium, std::move(established_connection)});
  current_medium_ = medium;
}

void AnalyticsRecorder::LogicalConnection::PhysicalConnectionClosed(
    Medium medium, DisconnectionReason reason, SafeDisconnectionResult result) {
  if (current_medium_ == UNKNOWN_MEDIUM) {
    LOG(WARNING) << "Unexpected call to PhysicalConnectionClosed() for medium  "
                 << Medium_Name(medium)
                 << " while AnalyticsRecorder has no active current medium";
  } else if (current_medium_ != medium) {
    LOG(WARNING) << "Unexpected call to PhysicalConnectionClosed() for medium "
                 << Medium_Name(medium)
                 << "while AnalyticsRecorder has active medium "
                 << Medium_Name(current_medium_);
  }

  auto it = physical_connections_.find(medium);
  if (it == physical_connections_.end()) {
    LOG(WARNING)
        << "Unexpected call to physicalConnectionClosed() for medium "
        << Medium_Name(medium)
        << " with no corresponding EstablishedConnection that was previously"
           " opened.";
    return;
  }
  ConnectionsLog::EstablishedConnection *established_connection =
      it->second.get();
  if (established_connection->has_disconnection_reason()) {
    LOG(WARNING) << "Unexpected call to physicalConnectionClosed() for medium "
                 << Medium_Name(medium)
                 << " which already has disconnection reason "
                 << DisconnectionReason_Name(
                        established_connection->disconnection_reason());
    return;
  }
  FinishPhysicalConnection(established_connection, reason, result);

  if (medium == current_medium_) {
    // If the EstablishedConnection we just closed was the one that we have
    // marked as current, unset currentMedium.
    current_medium_ = UNKNOWN_MEDIUM;
  }
}

void AnalyticsRecorder::LogicalConnection::CloseAllPhysicalConnections() {
  for (const auto &physical_connection : physical_connections_) {
    ConnectionsLog::EstablishedConnection *established_connection =
        physical_connection.second.get();
    if (!established_connection->has_disconnection_reason()) {
      FinishPhysicalConnection(
          established_connection, UNFINISHED,
          ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
    }
  }
  current_medium_ = UNKNOWN_MEDIUM;
}

std::vector<ConnectionsLog::EstablishedConnection>
AnalyticsRecorder::LogicalConnection::GetEstablisedConnections() {
  std::vector<ConnectionsLog::EstablishedConnection> established_connections;
  if (current_medium_ != UNKNOWN_MEDIUM) {
    LOG(WARNING)
        << "AnalyticsRecorder expected no more active physical connections "
           "before logging this endpoint connection.";
    return established_connections;
  }
  std::transform(physical_connections_.begin(), physical_connections_.end(),
                 std::back_inserter(established_connections),
                 [](auto &kv) { return *kv.second; });
  physical_connections_.clear();

  for (auto &established_connection : established_connections) {
    if (absl::Milliseconds(established_connection.duration_millis()) >=
        kConnectionTokenMaxLife) {
      LOG(INFO) << "connection token exceed TTL, drop token.";
      established_connection.set_connection_token("");
    }
  }

  return established_connections;
}

void AnalyticsRecorder::LogicalConnection::IncomingPayloadStarted(
    std::int64_t payload_id, PayloadType type, std::int64_t total_size_bytes) {
  incoming_payloads_.insert(
      {payload_id, std::make_unique<PendingPayload>(type, total_size_bytes,
                                                    no_record_time_millis_)});
}

void AnalyticsRecorder::LogicalConnection::ChunkReceived(
    std::int64_t payload_id, std::int64_t size_bytes) {
  auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end()) {
    return;
  }
  PendingPayload *pending_payload = it->second.get();
  pending_payload->AddChunk(size_bytes);
}

void AnalyticsRecorder::LogicalConnection::IncomingPayloadDone(
    std::int64_t payload_id, PayloadStatus status,
    OperationResultCode operation_result_code) {
  if (current_medium_ == UNKNOWN_MEDIUM) {
    LOG(WARNING) << "Unexpected call to incomingPayloadDone() while "
                    "AnalyticsRecorder has no active current medium.";
    return;
  }
  auto it = physical_connections_.find(current_medium_);
  if (it != physical_connections_.end()) {
    const std::unique_ptr<ConnectionsLog::EstablishedConnection>
        &established_connection = it->second;
    auto it = incoming_payloads_.find(payload_id);
    if (it != incoming_payloads_.end()) {
      it->second->SetOperationResultCode(operation_result_code);
      *established_connection->add_received_payload() =
          it->second->GetProtoPayload(status);
      incoming_payloads_.erase(it);
    }
  }
}

void AnalyticsRecorder::LogicalConnection::OutgoingPayloadStarted(
    std::int64_t payload_id, PayloadType type, std::int64_t total_size_bytes) {
  outgoing_payloads_.insert(
      {payload_id, std::make_unique<PendingPayload>(type, total_size_bytes,
                                                    no_record_time_millis_)});
}

void AnalyticsRecorder::LogicalConnection::ChunkSent(std::int64_t payload_id,
                                                     std::int64_t size_bytes) {
  auto it = outgoing_payloads_.find(payload_id);
  if (it == outgoing_payloads_.end()) {
    return;
  }
  PendingPayload *payload = it->second.get();
  payload->AddChunk(size_bytes);
}

void AnalyticsRecorder::LogicalConnection::OutgoingPayloadDone(
    std::int64_t payload_id, PayloadStatus status,
    OperationResultCode operation_result_code) {
  if (current_medium_ == UNKNOWN_MEDIUM) {
    LOG(WARNING) << "Unexpected call to outgoingPayloadDone() while "
                    "AnalyticsRecorder has no active current medium.";
    return;
  }
  auto it = physical_connections_.find(current_medium_);
  if (it != physical_connections_.end()) {
    const std::unique_ptr<ConnectionsLog::EstablishedConnection>
        &established_connection = it->second;
    auto it = outgoing_payloads_.find(payload_id);
    if (it != outgoing_payloads_.end()) {
      it->second->SetOperationResultCode(operation_result_code);
      *established_connection->add_sent_payload() =
          it->second->GetProtoPayload(status);
      outgoing_payloads_.erase(it);
    }
  }
}

void AnalyticsRecorder::LogicalConnection::FinishPhysicalConnection(
    ConnectionsLog::EstablishedConnection *established_connection,
    DisconnectionReason reason, SafeDisconnectionResult result) {
  established_connection->set_disconnection_reason(reason);
  established_connection->set_safe_disconnection_result(result);
  if (!no_record_time_millis_) {
    established_connection->set_duration_millis(
        absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
        established_connection->duration_millis());
  }

  // Add any not-yet-finished payloads to this EstablishedConnection.
  std::vector<ConnectionsLog::Payload> in_payloads =
      ResolvePendingPayloads(incoming_payloads_, reason);
  absl::c_move(in_payloads,
               RepeatedFieldBackInserter(
                   established_connection->mutable_received_payload()));
  std::vector<ConnectionsLog::Payload> out_payloads =
      ResolvePendingPayloads(outgoing_payloads_, reason);
  absl::c_move(out_payloads,
               RepeatedFieldBackInserter(
                   established_connection->mutable_sent_payload()));
}

std::vector<ConnectionsLog::Payload>
AnalyticsRecorder::LogicalConnection::ResolvePendingPayloads(
    absl::btree_map<std::int64_t, std::unique_ptr<PendingPayload>>
        &pending_payloads,
    DisconnectionReason reason) {
  std::vector<ConnectionsLog::Payload> completed_payloads;
  absl::btree_map<std::int64_t, std::unique_ptr<PendingPayload>>
      upgraded_payloads;
  PayloadStatus status =
      reason == UPGRADED ? MOVED_TO_NEW_MEDIUM : CONNECTION_CLOSED;

  OperationResultCode operation_result_code =
      GetPendingPayloadResultCodeFromReason(reason);
  for (const auto &item : pending_payloads) {
    const std::unique_ptr<PendingPayload> &pending_payload = item.second;
    pending_payload->SetOperationResultCode(operation_result_code);
    ConnectionsLog::Payload proto_payload =
        pending_payload->GetProtoPayload(status);
    completed_payloads.push_back(proto_payload);
    if (reason == UPGRADED) {
      upgraded_payloads.insert(
          {item.first,
           std::make_unique<PendingPayload>(
               pending_payload->type(), pending_payload->total_size_bytes(),
               no_record_time_millis_, operation_result_code)});
    }
  }
  pending_payloads.clear();

  if (reason == UPGRADED) {
    // Re-populate the map with a new PendingPayload for each pending payload,
    // since we expect them to be completed on the next EstablishedConnection.
    pending_payloads = std::move(upgraded_payloads);
  }
  // Return the list of completed payloads to be added to the current
  // EstablishedConnection.
  return completed_payloads;
}

OperationResultCode
AnalyticsRecorder::LogicalConnection::GetPendingPayloadResultCodeFromReason(
    DisconnectionReason reason) {
  switch (reason) {
    case UPGRADED:
      return OperationResultCode::MISCELLEANEOUS_MOVE_TO_NEW_MEDIUM;
    case DisconnectionReason::LOCAL_DISCONNECTION:
      return OperationResultCode::CLIENT_CANCELLATION_LOCAL_DISCONNECT;
    case DisconnectionReason::REMOTE_DISCONNECTION:
      return OperationResultCode::CLIENT_CANCELLATION_REMOTE_DISCONNECT;
    default:
      return OperationResultCode::NEARBY_GENERIC_CONNECTION_CLOSED;
  }
}

OperationResultCategory AnalyticsRecorder::GetOperationResultCategory(
    location::nearby::proto::connections::OperationResultCode result_code) {
  return ConvertToOperationResultCategory(result_code);
}

}  // namespace analytics
}  // namespace nearby
