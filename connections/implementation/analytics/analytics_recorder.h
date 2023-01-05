// Copyright 2022 Google LLC
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

#ifndef ANALYTICS_ANALYTICS_RECORDER_H_
#define ANALYTICS_ANALYTICS_RECORDER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/payload.h"
#include "connections/strategy.h"
#include "internal/analytics/event_logger.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace analytics {

class AnalyticsRecorder {
 public:
  explicit AnalyticsRecorder(::nearby::analytics::EventLogger *event_logger);
  virtual ~AnalyticsRecorder();

  // TODO(edwinwu): Implement to pass real values for AdvertisingMetadata and
  // DiscoveryMetaData: is_extended_advertisement_supported,
  // connected_ap_frequency, and is_nfc_available. Set as default values for
  // analytics recorder.
  // Advertising phase
  void OnStartAdvertising(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium> &mediums,
      bool is_extended_advertisement_supported = false,
      int connected_ap_frequency = 0, bool is_nfc_available = false)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnStopAdvertising() ABSL_LOCKS_EXCLUDED(mutex_);

  // Discovery phase
  void OnStartDiscovery(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium> &mediums,
      bool is_extended_advertisement_supported = false,
      int connected_ap_frequency = 0, bool is_nfc_available = false)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnStopDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);
  void OnEndpointFound(location::nearby::proto::connections::Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connection request
  void OnConnectionRequestReceived(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnConnectionRequestSent(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnRemoteEndpointAccepted(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnLocalEndpointAccepted(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnRemoteEndpointRejected(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnLocalEndpointRejected(const std::string &remote_endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connection attempt
  void OnIncomingConnectionAttempt(
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string &connection_token,
      ConnectionAttemptMetadataParams *connection_attempt_metadata_params)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutgoingConnectionAttempt(
      const std::string &remote_endpoint_id,
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string &connection_token,
      ConnectionAttemptMetadataParams *connection_attempt_metadata_params)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // TODO(edwinwu): Implement network operator, country code, tdls, wifi hotspot
  //, max wifi tx/rx speed and channel width. Set as default values for
  // analytics recorder.
  static std::unique_ptr<ConnectionAttemptMetadataParams>
  BuildConnectionAttemptMetadataParams(
      location::nearby::proto::connections::ConnectionTechnology technology,
      location::nearby::proto::connections::ConnectionBand band, int frequency,
      int try_count, const std::string &network_operator = {},
      const std::string &country_code = {}, bool is_tdls_used = false,
      bool wifi_hotspot_enabled = false, int max_wifi_tx_speed = 0,
      int max_wifi_rx_speed = 0, int channel_width = -1);

  // Connection established
  void OnConnectionEstablished(
      const std::string &endpoint_id,
      location::nearby::proto::connections::Medium medium,
      const std::string &connection_token) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnConnectionClosed(
      const std::string &endpoint_id,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections ::DisconnectionReason reason)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Payload
  void OnIncomingPayloadStarted(const std::string &endpoint_id,
                                std::int64_t payload_id,
                                connections::PayloadType type,
                                std::int64_t total_size_bytes)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnPayloadChunkReceived(const std::string &endpoint_id,
                              std::int64_t payload_id,
                              std::int64_t chunk_size_bytes)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnIncomingPayloadDone(
      const std::string &endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutgoingPayloadStarted(const std::vector<std::string> &endpoint_ids,
                                std::int64_t payload_id,
                                connections::PayloadType type,
                                std::int64_t total_size_bytes)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnPayloadChunkSent(const std::string &endpoint_id,
                          std::int64_t payload_id,
                          std::int64_t chunk_size_bytes)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutgoingPayloadDone(
      const std::string &endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // BandwidthUpgrade
  void OnBandwidthUpgradeStarted(
      const std::string &endpoint_id,
      location::nearby::proto::connections::Medium from_medium,
      location::nearby::proto::connections::Medium to_medium,
      location::nearby::proto::connections::ConnectionAttemptDirection
          direction,
      const std::string &connection_token) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnBandwidthUpgradeError(
      const std::string &endpoint_id,
      location::nearby::proto::connections::BandwidthUpgradeResult result,
      location::nearby::proto::connections::BandwidthUpgradeErrorStage
          error_stage) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnBandwidthUpgradeSuccess(const std::string &endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Error Code
  void OnErrorCode(const ErrorCodeParams &params);

  // Log the start client session event with start client session logging
  // resouces setup (e.g. client_session_, started_client_session_time_)
  void LogStartSession() ABSL_LOCKS_EXCLUDED(mutex_);

  // Invokes event_logger_.Log() at the end of life of client. Log action is
  // called in a separate thread to allow synchronous potentially lengthy
  // execution.
  void LogSession() ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsSessionLogged();

 private:
  // Tracks the chunks and duration of a Payload on a particular medium.
  class PendingPayload {
   public:
    PendingPayload(location::nearby::proto::connections::PayloadType type,
                   std::int64_t total_size_bytes)
        : start_time_(SystemClock::ElapsedRealtime()),
          type_(type),
          total_size_bytes_(total_size_bytes),
          num_bytes_transferred_(0),
          num_chunks_(0) {}
    ~PendingPayload() = default;

    void AddChunk(std::int64_t chunk_size_bytes);

    location::nearby::analytics::proto::ConnectionsLog::Payload GetProtoPayload(
        location::nearby::proto::connections::PayloadStatus status);

    location::nearby::proto::connections::PayloadType type() const {
      return type_;
    }

    std::int64_t total_size_bytes() const { return total_size_bytes_; }

   private:
    absl::Time start_time_;
    location::nearby::proto::connections::PayloadType type_;
    std::int64_t total_size_bytes_;
    std::int64_t num_bytes_transferred_;
    int num_chunks_;
  };

  class LogicalConnection {
   public:
    LogicalConnection(
        location::nearby::proto::connections::Medium initial_medium,
        const std::string &connection_token) {
      PhysicalConnectionEstablished(initial_medium, connection_token);
    }
    LogicalConnection(const LogicalConnection &) = delete;
    LogicalConnection(LogicalConnection &&other)
        : current_medium_(std::move(other.current_medium_)),
          physical_connections_(std::move(other.physical_connections_)),
          incoming_payloads_(std::move(other.incoming_payloads_)),
          outgoing_payloads_(std::move(other.outgoing_payloads_)) {}
    LogicalConnection &operator=(const LogicalConnection &) = delete;
    LogicalConnection &&operator=(LogicalConnection &&) = delete;
    ~LogicalConnection() = default;

    void PhysicalConnectionEstablished(
        location::nearby::proto::connections::Medium medium,
        const std::string &connection_token);
    void PhysicalConnectionClosed(
        location::nearby::proto::connections::Medium medium,
        location::nearby::proto::connections::DisconnectionReason reason);
    void CloseAllPhysicalConnections();

    void IncomingPayloadStarted(
        std::int64_t payload_id,
        location::nearby::proto::connections::PayloadType type,
        std::int64_t total_size_bytes);
    void ChunkReceived(std::int64_t payload_id, std::int64_t size_bytes);
    void IncomingPayloadDone(
        std::int64_t payload_id,
        location::nearby::proto::connections::PayloadStatus status);
    void OutgoingPayloadStarted(
        std::int64_t payload_id,
        location::nearby::proto::connections::PayloadType type,
        std::int64_t total_size_bytes);
    void ChunkSent(std::int64_t payload_id, std::int64_t size_bytes);
    void OutgoingPayloadDone(
        std::int64_t payload_id,
        location::nearby::proto::connections::PayloadStatus status);

    std::vector<location::nearby::analytics::proto::ConnectionsLog::
                    EstablishedConnection>
    GetEstablisedConnections();

   private:
    void FinishPhysicalConnection(
        location::nearby::analytics::proto::ConnectionsLog::
            EstablishedConnection *established_connection,
        location::nearby::proto::connections::DisconnectionReason reason);
    std::vector<location::nearby::analytics::proto::ConnectionsLog::Payload>
    ResolvePendingPayloads(
        absl::btree_map<std::int64_t, std::unique_ptr<PendingPayload>>
            &pending_payloads,
        location::nearby::proto::connections::DisconnectionReason reason);

    location::nearby::proto::connections::Medium current_medium_ =
        location::nearby::proto::connections::UNKNOWN_MEDIUM;
    absl::btree_map<location::nearby::proto::connections::Medium,
                    std::unique_ptr<location::nearby::analytics::proto::
                                        ConnectionsLog::EstablishedConnection>>
        physical_connections_;
    absl::btree_map<std::int64_t, std::unique_ptr<PendingPayload>>
        incoming_payloads_;
    absl::btree_map<std::int64_t, std::unique_ptr<PendingPayload>>
        outgoing_payloads_;
  };

  bool CanRecordAnalyticsLocked(absl::string_view method_name)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // Callbacks the ConnectionsLog proto byte array data to the EventLogger with
  // ClientSession sub-proto.
  void LogClientSession();
  // Callbacks the ConnectionsLog proto byte array data to the EventLogger.
  void LogEvent(location::nearby::proto::connections::EventType event_type);

  void UpdateStrategySessionLocked(
      connections::Strategy strategy,
      location::nearby::proto::connections::SessionRole role)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RecordAdvertisingPhaseDurationLocked() const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishAdvertisingPhaseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RecordDiscoveryPhaseDurationLocked() const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishDiscoveryPhaseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool UpdateAdvertiserConnectionRequestLocked(
      location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  bool UpdateDiscovererConnectionRequestLocked(
      location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  bool BothEndpointsRespondedLocked(
      location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void LocalEndpointRespondedLocked(
      const std::string &remote_endpoint_id,
      location::nearby::proto::connections::ConnectionRequestResponse response)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RemoteEndpointRespondedLocked(
      const std::string &remote_endpoint_id,
      location::nearby::proto::connections::ConnectionRequestResponse response)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void MarkConnectionRequestIgnoredLocked(
      location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishUpgradeAttemptLocked(
      const std::string &endpoint_id,
      location::nearby::proto::connections::BandwidthUpgradeResult result,
      location::nearby::proto::connections::BandwidthUpgradeErrorStage
          error_stage,
      bool erase_item = true) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishStrategySessionLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Reset the client cession's logging resources (e.g. current_strategy_,
  // current_advertising_phase_, current_discovery_phase_, etc)
  void ResetClientSessionLoggingResouces();

  location::nearby::proto::connections::ConnectionsStrategy
  StrategyToConnectionStrategy(connections::Strategy strategy);
  location::nearby::proto::connections::PayloadType
  PayloadTypeToProtoPayloadType(connections::PayloadType type);

  // Not owned by AnalyticsRecorder. Pointer must refer to a valid object
  // that outlives the one constructed.
  ::nearby::analytics::EventLogger *event_logger_;

  SingleThreadExecutor serial_executor_;
  // Protects all sub-protos reading and writing in ConnectionLog.
  Mutex mutex_;

  // ClientSession
  std::unique_ptr<
      location::nearby::analytics::proto::ConnectionsLog::ClientSession>
      client_session_;
  absl::Time started_client_session_time_;
  bool session_was_logged_ ABSL_GUARDED_BY(mutex_) = false;
  bool start_client_session_was_logged_ ABSL_GUARDED_BY(mutex_) = false;

  // Current StrategySession
  connections::Strategy current_strategy_ ABSL_GUARDED_BY(mutex_) =
      connections::Strategy::kNone;
  std::unique_ptr<
      location::nearby::analytics::proto::ConnectionsLog::StrategySession>
      current_strategy_session_ ABSL_GUARDED_BY(mutex_);
  absl::Time started_strategy_session_time_ ABSL_GUARDED_BY(mutex_);

  // Current AdvertisingPhase
  std::unique_ptr<
      location::nearby::analytics::proto::ConnectionsLog::AdvertisingPhase>
      current_advertising_phase_;
  absl::Time started_advertising_phase_time_;

  // Current DiscoveryPhase
  std::unique_ptr<
      location::nearby::analytics::proto::ConnectionsLog::DiscoveryPhase>
      current_discovery_phase_;
  absl::Time started_discovery_phase_time_;

  absl::btree_map<std::string,
                  std::unique_ptr<location::nearby::analytics::proto::
                                      ConnectionsLog::ConnectionRequest>>
      incoming_connection_requests_ ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string,
                  std::unique_ptr<location::nearby::analytics::proto::
                                      ConnectionsLog::ConnectionRequest>>
      outgoing_connection_requests_ ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string, std::unique_ptr<LogicalConnection>>
      active_connections_ ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string,
                  std::unique_ptr<location::nearby::analytics::proto::
                                      ConnectionsLog::BandwidthUpgradeAttempt>>
      bandwidth_upgrade_attempts_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace analytics
}  // namespace nearby

#endif  // ANALYTICS_ANALYTICS_RECORDER_H_
