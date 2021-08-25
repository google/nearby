// Copyright 2020 Google LLC
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

#include <string>

#include "absl/container/btree_map.h"
#include "absl/time/time.h"
#include "core/event_logger.h"
#include "core/strategy.h"
#include "platform/public/mutex.h"
#include "platform/public/single_thread_executor.h"
#include "proto/analytics/connections_log.proto.h"
#include "proto/connections_enums.proto.h"

namespace location {
namespace nearby {
namespace analytics {

class AnalyticsRecorder {
 public:
  static constexpr absl::string_view kVersion = "v1.0.0";

  explicit AnalyticsRecorder(EventLogger *event_logger);
  virtual ~AnalyticsRecorder();

  // Advertising phase
  void OnStartAdvertising(
      connections::Strategy strategy,
      const std::vector<::location::nearby::proto::connections::Medium>
          &mediums) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnStopAdvertising() ABSL_LOCKS_EXCLUDED(mutex_);

  // Discovery phase
  void OnStartDiscovery(
      connections::Strategy strategy,
      const std::vector<::location::nearby::proto::connections::Medium>
          &mediums) ABSL_LOCKS_EXCLUDED(mutex_);
  void OnStopDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);
  void OnEndpointFound(::location::nearby::proto::connections::Medium medium)
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
      ::location::nearby::proto::connections::ConnectionAttemptType type,
      ::location::nearby::proto::connections::Medium medium,
      ::location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string &connection_token)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutgoingConnectionAttempt(
      const std::string &remote_endpoint_id,
      ::location::nearby::proto::connections::ConnectionAttemptType type,
      ::location::nearby::proto::connections::Medium medium,
      ::location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string &connection_token)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Invokes event_logger_.Log() at the end of life of client. Log action is
  // called in a separate thread to allow synchronous potentially lengthy
  // execution.
  void LogSession() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  bool CanRecordAnalyticsLocked(const std::string &method_name)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // Callbacks the ConnectionsLog proto byte array data to the EventLogger with
  // ClientSession sub-proto.
  void LogClientSession();
  // Callbacks the ConnectionsLog proto byte array data to the EventLogger.
  void LogEvent(::location::nearby::proto::connections::EventType event_type);

  void UpdateStrategySessionLocked(
      connections::Strategy strategy,
      ::location::nearby::proto::connections::SessionRole role)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RecordAdvertisingPhaseDurationLocked() const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishAdvertisingPhaseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RecordDiscoveryPhaseDurationLocked() const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishDiscoveryPhaseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool UpdateAdvertiserConnectionRequestLocked(
      ::location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  bool UpdateDiscovererConnectionRequestLocked(
      ::location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  bool BothEndpointsRespondedLocked(
      ::location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void LocalEndpointRespondedLocked(
      const std::string &remote_endpoint_id,
      ::location::nearby::proto::connections::ConnectionRequestResponse
          response) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void RemoteEndpointRespondedLocked(
      const std::string &remote_endpoint_id,
      ::location::nearby::proto::connections::ConnectionRequestResponse
          response) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void MarkConnectionRequestIgnoredLocked(
      ::location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest
          *request) ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void FinishStrategySessionLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ::location::nearby::proto::connections::ConnectionsStrategy
  StrategyToConnectionStrategy(connections::Strategy strategy);

  // Not owned by AnalyticsRecorder. Pointer must refer to a valid object
  // that outlives the one constructed.
  EventLogger *event_logger_;

  SingleThreadExecutor serial_executor_;
  // Protects all sub-protos reading and writing in ConnectionLog.
  Mutex mutex_;

  // ClientSession
  std::unique_ptr<
      ::location::nearby::analytics::proto::ConnectionsLog::ClientSession>
      client_session_ = std::make_unique<::location::nearby::analytics::proto::
                                             ConnectionsLog::ClientSession>();
  absl::Time started_client_session_time_;
  bool session_was_logged_ ABSL_GUARDED_BY(mutex_) = false;

  // Current StrategySession
  connections::Strategy current_strategy_ ABSL_GUARDED_BY(mutex_) =
      connections::Strategy::kNone;
  std::unique_ptr<
      ::location::nearby::analytics::proto::ConnectionsLog::StrategySession>
      current_strategy_session_ ABSL_GUARDED_BY(mutex_);
  absl::Time started_strategy_session_time_ ABSL_GUARDED_BY(mutex_);

  // Current AdvertisingPhase
  std::unique_ptr<
      ::location::nearby::analytics::proto::ConnectionsLog::AdvertisingPhase>
      current_advertising_phase_;
  absl::Time started_advertising_phase_time_;

  // Current DiscoveryPhase
  std::unique_ptr<
      ::location::nearby::analytics::proto::ConnectionsLog::DiscoveryPhase>
      current_discovery_phase_;
  absl::Time started_discovery_phase_time_;

  absl::btree_map<std::string,
                  std::unique_ptr<::location::nearby::analytics::proto::
                                      ConnectionsLog::ConnectionRequest>>
      incoming_connection_requests_ ABSL_GUARDED_BY(mutex_);
  absl::btree_map<std::string,
                  std::unique_ptr<::location::nearby::analytics::proto::
                                      ConnectionsLog::ConnectionRequest>>
      outgoing_connection_requests_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace analytics
}  // namespace nearby
}  // namespace location

#endif  // ANALYTICS_ANALYTICS_RECORDER_H_
