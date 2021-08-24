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

#include "third_party/nearby_connections/cpp/analytics/analytics_recorder.h"

#include <string>
#include <utility>

#include "absl/time/time.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"
#include "platform/public/system_clock.h"
#include "proto/analytics/connections_log.proto.h"
#include "proto/connections_enums.proto.h"

namespace location {
namespace nearby {
namespace analytics {

using ConnectionsLog = ::location::nearby::analytics::proto::ConnectionsLog;
using ClientSession = ConnectionsLog::ClientSession;
using StrategySession = ConnectionsLog::StrategySession;
using AdvertisingPhase = ConnectionsLog::AdvertisingPhase;
using DiscoveryPhase = ConnectionsLog::DiscoveryPhase;
using DiscoveredEndpoint = ConnectionsLog::DiscoveredEndpoint;
using ConnectionRequest = ConnectionsLog::ConnectionRequest;
using ConnectionRequestResponse =
    ::location::nearby::proto::connections::ConnectionRequestResponse;
using ConnectionStrategy =
    ::location::nearby::proto::connections::ConnectionsStrategy;
using EventType = ::location::nearby::proto::connections::EventType;
using Medium = ::location::nearby::proto::connections::Medium;
using SessionRole = ::location::nearby::proto::connections::SessionRole;

// These definitions are necessary before C++17.
constexpr absl::string_view AnalyticsRecorder::kVersion;

AnalyticsRecorder::AnalyticsRecorder(EventLogger *event_logger)
    : event_logger_(event_logger) {
  started_client_session_time_ = SystemClock::ElapsedRealtime();
  NEARBY_LOGS(INFO) << "AnalyticsRecorder ctor event_logger_=" << event_logger_;
  MutexLock lock(&mutex_);
  if (CanRecordAnalyticsLocked("OnStartClientSession")) {
    LogEvent(::location::nearby::proto::connections::START_CLIENT_SESSION);
  }
}

AnalyticsRecorder::~AnalyticsRecorder() {
  MutexLock lock(&mutex_);
  incoming_connection_requests_.clear();
  outgoing_connection_requests_.clear();
  serial_executor_.Shutdown();
}

void AnalyticsRecorder::OnStartAdvertising(connections::Strategy strategy,
                                           const std::vector<Medium> &mediums) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStartAdvertising")) {
    return;
  }
  if (!strategy.IsValid()) {
    NEARBY_LOGS(INFO) << "AnalyticsRecorder OnStartAdvertising with unknown "
                         "strategy, bail out.";
    return;
  }
  // Initialize/update a StrategySession.
  UpdateStrategySessionLocked(
      strategy, ::location::nearby::proto::connections::ADVERTISER);

  // Initialize and set a AdvertisingPhase.
  started_advertising_phase_time_ = SystemClock::ElapsedRealtime();
  current_advertising_phase_ = std::make_unique<AdvertisingPhase>();
  absl::c_copy(mediums, RepeatedFieldBackInserter(
                            current_advertising_phase_->mutable_medium()));
}

void AnalyticsRecorder::OnStopAdvertising() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStopAdvertising")) {
    return;
  }
  RecordAdvertisingPhaseDurationLocked();
}
void AnalyticsRecorder::OnStartDiscovery(connections::Strategy strategy,
                                         const std::vector<Medium> &mediums) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStartDiscovery")) {
    return;
  }
  if (!strategy.IsValid()) {
    NEARBY_LOGS(INFO) << "AnalyticsRecorder OnStartDiscovery unknown "
                         "strategy enter, bail out.";
    return;
  }

  // Initialize/update a StrategySession.
  UpdateStrategySessionLocked(
      strategy, ::location::nearby::proto::connections::DISCOVERER);

  // Initialize and set a DiscoveryPhase.
  started_discovery_phase_time_ = SystemClock::ElapsedRealtime();
  current_discovery_phase_ = std::make_unique<DiscoveryPhase>();
  for (auto medium : mediums) {
    current_discovery_phase_->add_medium(medium);
  }
}

void AnalyticsRecorder::OnStopDiscovery() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnStopDiscovery")) {
    return;
  }
  RecordDiscoveryPhaseDurationLocked();
}

void AnalyticsRecorder::OnEndpointFound(Medium medium) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnEndpointFound")) {
    return;
  }
  if (current_discovery_phase_ == nullptr) {
    NEARBY_LOGS(INFO) << "Unable to record discovered endpoint due to null "
                         "current_discovery_phase_";
    return;
  }
  DiscoveredEndpoint *discovered_endpoint =
      current_discovery_phase_->add_discovered_endpoint();
  discovered_endpoint->set_medium(medium);
  discovered_endpoint->set_latency_millis(absl::ToInt64Milliseconds(
      SystemClock::ElapsedRealtime() - started_discovery_phase_time_));
}

void AnalyticsRecorder::OnConnectionRequestReceived(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnConnectionRequestReceived")) {
    return;
  }
  absl::Time current_time = SystemClock::ElapsedRealtime();
  auto connection_request(std::make_unique<ConnectionRequest>());
  connection_request->set_duration_millis(absl::ToUnixMillis(current_time));
  connection_request->set_request_delay_millis(absl::ToInt64Milliseconds(
      current_time - started_advertising_phase_time_));
  incoming_connection_requests_.emplace(remote_endpoint_id,
                                        std::move(connection_request));
}

void AnalyticsRecorder::OnConnectionRequestSent(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnConnectionRequestSent")) {
    return;
  }
  absl::Time current_time = SystemClock::ElapsedRealtime();
  auto connection_request(std::make_unique<ConnectionRequest>());
  connection_request->set_duration_millis(absl::ToUnixMillis(current_time));
  connection_request->set_request_delay_millis(
      absl::ToInt64Milliseconds(current_time - started_discovery_phase_time_));
  outgoing_connection_requests_.emplace(remote_endpoint_id,
                                        std::move(connection_request));
}

void AnalyticsRecorder::OnRemoteEndpointAccepted(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnRemoteEndpointAccepted")) {
    return;
  }
  RemoteEndpointRespondedLocked(
      remote_endpoint_id, ::location::nearby::proto::connections::ACCEPTED);
}

void AnalyticsRecorder::OnLocalEndpointAccepted(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnLocalEndpointAccepted")) {
    return;
  }
  LocalEndpointRespondedLocked(
      remote_endpoint_id, ::location::nearby::proto::connections::ACCEPTED);
}

void AnalyticsRecorder::OnRemoteEndpointRejected(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnRemoteEndpointRejected")) {
    return;
  }
  RemoteEndpointRespondedLocked(
      remote_endpoint_id, ::location::nearby::proto::connections::REJECTED);
}

void AnalyticsRecorder::OnLocalEndpointRejected(
    const std::string &remote_endpoint_id) {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("OnLocalEndpointRejected")) {
    return;
  }
  LocalEndpointRespondedLocked(
      remote_endpoint_id, ::location::nearby::proto::connections::REJECTED);
}

void AnalyticsRecorder::LogSession() {
  MutexLock lock(&mutex_);
  if (!CanRecordAnalyticsLocked("LogSession")) {
    return;
  }
  FinishStrategySessionLocked();
  client_session_->set_duration_millis(absl::ToInt64Milliseconds(
      SystemClock::ElapsedRealtime() - started_client_session_time_));
  LogClientSession();
  LogEvent(::location::nearby::proto::connections::STOP_CLIENT_SESSION);
  session_was_logged_ = true;
}

bool AnalyticsRecorder::CanRecordAnalyticsLocked(
    const std::string &method_name) {
  NEARBY_LOGS(VERBOSE) << "AnalyticsRecorder LogEvent " << method_name
                       << " is calling.";
  if (event_logger_ == nullptr) {
    NEARBY_LOGS(WARNING)
        << "AnalyticsRecorder CanRecordAnalytics Unexpected call "
        << method_name << " due to event_logger is null.";
    return false;
  }

  if (session_was_logged_) {
    NEARBY_LOGS(WARNING)
        << "AnalyticsRecorder CanRecordAnalytics Unexpected call "
        << method_name << " after session has already been logged.";
    return false;
  }
  return true;
}

void AnalyticsRecorder::LogClientSession() {
  serial_executor_.Execute(
      "analytics-recorder", [this]() {
        ConnectionsLog connections_log;
        connections_log.set_event_type(
            ::location::nearby::proto::connections::CLIENT_SESSION);
        connections_log.set_allocated_client_session(client_session_.release());
        connections_log.set_version(kVersion);

        NEARBY_LOGS(VERBOSE)
            << "AnalyticsRecorder LogClientSession connections_log="
            << connections_log.DebugString();

        event_logger_->Log(connections_log);
      });
}

void AnalyticsRecorder::LogEvent(EventType event_type) {
  serial_executor_.Execute("analytics-recorder", [this, event_type]() {
    ConnectionsLog connections_log;
    connections_log.set_event_type(event_type);
    connections_log.set_version(kVersion);

    NEARBY_LOGS(VERBOSE) << "AnalyticsRecorder LogEvent connections_log="
                         << connections_log.DebugString();

    event_logger_->Log(connections_log);
  });
}

void AnalyticsRecorder::UpdateStrategySessionLocked(
    connections::Strategy strategy, SessionRole role) {
  // If we're not switching strategies, just update the current StrategySession
  // with the new role.
  if (strategy == current_strategy_) {
    if (absl::c_linear_search(current_strategy_session_->role(), role)) {
      // We've already acted as this role before, so make sure we've finished
      // recording the previous round.
      switch (role) {
        case ::location::nearby::proto::connections::ADVERTISER:
          FinishAdvertisingPhaseLocked();
          break;
        case ::location::nearby::proto::connections::DISCOVERER:
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
    LogEvent(::location::nearby::proto::connections::START_STRATEGY_SESSION);
    current_strategy_session_ = std::make_unique<StrategySession>();
    started_strategy_session_time_ = SystemClock::ElapsedRealtime();
    current_strategy_session_->set_strategy(
        StrategyToConnectionStrategy(strategy));
    current_strategy_session_->add_role(role);
  }
}

void AnalyticsRecorder::RecordAdvertisingPhaseDurationLocked() const {
  if (current_advertising_phase_ == nullptr) {
    NEARBY_LOGS(INFO) << "Unable to record advertising phase duration due to "
                         "null current_advertising_phase_";
    return;
  }
  if (!current_advertising_phase_->has_duration_millis()) {
    current_advertising_phase_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_advertising_phase_time_));
  }
}

void AnalyticsRecorder::FinishAdvertisingPhaseLocked() {
  if (current_advertising_phase_ != nullptr) {
    for (const auto &item : incoming_connection_requests_) {
      // ConnectionRequests still pending have been ignored by the local or
      // remote (or both) endpoints.
      auto &connection_request = item.second;
      MarkConnectionRequestIgnoredLocked(connection_request.get());
      UpdateAdvertiserConnectionRequestLocked(connection_request.get());
    }
    RecordAdvertisingPhaseDurationLocked();
    *current_strategy_session_->add_advertising_phase() =
        *std::move(current_advertising_phase_);
  }
  incoming_connection_requests_.clear();
}

void AnalyticsRecorder::RecordDiscoveryPhaseDurationLocked() const {
  if (current_discovery_phase_ == nullptr) {
    NEARBY_LOGS(INFO) << "Unable to record discovery phase duration due to "
                         "null current_discovery_phase_";
    return;
  }
  if (!current_discovery_phase_->has_duration_millis()) {
    current_discovery_phase_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - started_discovery_phase_time_));
  }
}

void AnalyticsRecorder::FinishDiscoveryPhaseLocked() {
  if (current_discovery_phase_ != nullptr) {
    for (const auto &item : outgoing_connection_requests_) {
      // ConnectionRequests still pending have been ignored by the local or
      // remote (or both) endpoints.
      auto &connection_request = item.second;
      MarkConnectionRequestIgnoredLocked(connection_request.get());
      UpdateDiscovererConnectionRequestLocked(connection_request.get());
    }
    RecordDiscoveryPhaseDurationLocked();
    *current_strategy_session_->add_discovery_phase() =
        *std::move(current_discovery_phase_);
  }
  outgoing_connection_requests_.clear();
}

bool AnalyticsRecorder::UpdateAdvertiserConnectionRequestLocked(
    ConnectionRequest *request) {
  if (current_advertising_phase_ == nullptr) {
    NEARBY_LOGS(INFO)
        << "Unable to record advertiser connection request due to null "
           "current_advertising_phase_";
    return false;
  }
  if (BothEndpointsRespondedLocked(request)) {
    request->set_duration_millis(
        absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
        request->duration_millis());
    *current_advertising_phase_->add_received_connection_request() =
        *std::move(request);
    return true;
  }
  return false;
}

bool AnalyticsRecorder::UpdateDiscovererConnectionRequestLocked(
    ConnectionRequest *request) {
  if (current_discovery_phase_ == nullptr) {
    NEARBY_LOGS(INFO) << "Unable to record discoverer connection request due "
                         "to null current_discovery_phase_.";
    return false;
  }
  if (BothEndpointsRespondedLocked(request) ||
      request->local_response() ==
          ::location::nearby::proto::connections::NOT_SENT) {
    request->set_duration_millis(
        absl::ToUnixMillis(SystemClock::ElapsedRealtime()) -
        request->duration_millis());
    *current_discovery_phase_->add_sent_connection_request() =
        *std::move(request);
    return true;
  }
  return false;
}

bool AnalyticsRecorder::BothEndpointsRespondedLocked(
    ConnectionRequest *request) {
  return request->has_local_response() && request->has_remote_response();
}

void AnalyticsRecorder::LocalEndpointRespondedLocked(
    const std::string &remote_endpoint_id, ConnectionRequestResponse response) {
  auto out = outgoing_connection_requests_.find(remote_endpoint_id);
  if (out != outgoing_connection_requests_.end()) {
    ConnectionRequest *connection_request = out->second.get();
    connection_request->set_local_response(response);
    if (UpdateDiscovererConnectionRequestLocked(connection_request)) {
      outgoing_connection_requests_.erase(out);
    }
  }
  auto in = incoming_connection_requests_.find(remote_endpoint_id);
  if (in != incoming_connection_requests_.end()) {
    ConnectionRequest *connection_request = in->second.get();
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
    ConnectionRequest *connection_request = out->second.get();
    connection_request->set_remote_response(response);
    if (UpdateDiscovererConnectionRequestLocked(connection_request)) {
      outgoing_connection_requests_.erase(out);
    }
  }
  auto in = incoming_connection_requests_.find(remote_endpoint_id);
  if (in != incoming_connection_requests_.end()) {
    ConnectionRequest *connection_request = in->second.get();
    connection_request->set_remote_response(response);
    if (UpdateAdvertiserConnectionRequestLocked(connection_request)) {
      incoming_connection_requests_.erase(in);
    }
  }
}

void AnalyticsRecorder::MarkConnectionRequestIgnoredLocked(
    ConnectionRequest *request) {
  if (!request->has_local_response()) {
    request->set_local_response(
        ::location::nearby::proto::connections::IGNORED);
  }
  if (!request->has_remote_response()) {
    request->set_remote_response(
        ::location::nearby::proto::connections::IGNORED);
  }
}

void AnalyticsRecorder::FinishStrategySessionLocked() {
  if (current_strategy_session_ != nullptr) {
    FinishAdvertisingPhaseLocked();
    FinishDiscoveryPhaseLocked();

    // Add the StrategySession in ClientSession
    current_strategy_session_->set_duration_millis(absl::ToInt64Milliseconds(
        SystemClock::ElapsedRealtime() - SystemClock::ElapsedRealtime()));
    *client_session_->add_strategy_session() =
        *std::move(current_strategy_session_);
    LogEvent(::location::nearby::proto::connections::STOP_STRATEGY_SESSION);
  }
}

ConnectionStrategy AnalyticsRecorder::StrategyToConnectionStrategy(
    connections::Strategy strategy) {
  if (strategy == connections::Strategy::kP2pCluster) {
    return ::location::nearby::proto::connections::P2P_CLUSTER;
  }
  if (strategy == connections::Strategy::kP2pStar) {
    return ::location::nearby::proto::connections::P2P_STAR;
  }
  if (strategy == connections::Strategy::kP2pPointToPoint) {
    return ::location::nearby::proto::connections::P2P_POINT_TO_POINT;
  }
  return ::location::nearby::proto::connections::UNKNOWN_STRATEGY;
}

}  // namespace analytics
}  // namespace nearby
}  // namespace location
