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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/logging.h"
#include "proto/analytics/connections_log.proto.h"
#include "proto/connections_enums.proto.h"

namespace location {
namespace nearby {
namespace analytics {
namespace {

using ::location::nearby::proto::connections::BLE;
using ::location::nearby::proto::connections::BLUETOOTH;
using ::location::nearby::proto::connections::CLIENT_SESSION;
using ::location::nearby::proto::connections::EventType;
using ::location::nearby::proto::connections::INITIAL;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::RESULT_ERROR;
using ::location::nearby::proto::connections::RESULT_SUCCESS;
using ::location::nearby::proto::connections::START_STRATEGY_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_STRATEGY_SESSION;
using ::testing::Contains;
using ::testing::EqualsProto;
using ::testing::proto::Partially;

constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);

class FakeEventLogger : public EventLogger {
 public:
  explicit FakeEventLogger(CountDownLatch& client_session_done_latch)
      : client_session_done_latch_(client_session_done_latch) {}

  void Log(const proto::ConnectionsLog& connections_log) override {
    EventType event_type = connections_log.event_type();
    logged_event_types_.push_back(event_type);
    if (event_type == CLIENT_SESSION) {
      logged_client_session_count_++;
      logged_client_session_ = connections_log.client_session();
    }
    if (event_type == STOP_CLIENT_SESSION) {
      client_session_done_latch_.CountDown();
    }
  }

  int GetLoggedClientSessionCount() const {
    return logged_client_session_count_;
  }

  const proto::ConnectionsLog::ClientSession& GetLoggedClientSession() {
    return logged_client_session_;
  }

  std::vector<EventType> GetLoggedEventTypes() { return logged_event_types_; }

 private:
  int logged_client_session_count_ = 0;
  CountDownLatch& client_session_done_latch_;
  proto::ConnectionsLog::ClientSession logged_client_session_;
  std::vector<EventType> logged_event_types_;
};

TEST(AnalyticsRecorderTest, SessionOnlyLoggedOnceWorks) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.LogSession();
  analytics_recorder.LogSession();
  analytics_recorder.LogSession();
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  // Only called once.
  EXPECT_EQ(event_logger.GetLoggedClientSessionCount(), 1);
}

TEST(AnalyticsRecorderTest, SetFieldsCorrectlyForNestedAdvertisingCalls) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnStopAdvertising();
  analytics_recorder.OnStartAdvertising(strategy, {BLUETOOTH});

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase < medium: BLE medium: BLUETOOTH >
                  advertising_phase < medium: BLUETOOTH >
                >)pb")));
}

TEST(AnalyticsRecorderTest, SetFieldsCorrectlyForNestedDiscoveryCalls) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnEndpointFound(BLUETOOTH);
  analytics_recorder.OnEndpointFound(BLE);
  analytics_recorder.OnStartDiscovery(strategy, {BLUETOOTH});

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: DISCOVERER
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    discovered_endpoint < medium: BLUETOOTH >
                    discovered_endpoint < medium: BLE >
                  >
                  discovery_phase < medium: BLUETOOTH >
                >)pb")));
}

TEST(AnalyticsRecorderTest,
     OneStrategySessionForMultipleRoundsOfDiscoveryAdvertising) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnStopAdvertising();
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnStopAdvertising();
  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnStopAdvertising();

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  std::vector<EventType> event_types = event_logger.GetLoggedEventTypes();
  EXPECT_THAT(event_types, Contains(START_STRATEGY_SESSION).Times(1));
  EXPECT_THAT(event_types, Contains(STOP_STRATEGY_SESSION).Times(1));

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  role: DISCOVERER
                  discovery_phase < medium: BLE medium: BLUETOOTH >
                  discovery_phase < medium: BLE medium: BLUETOOTH >
                  discovery_phase < medium: BLE medium: BLUETOOTH >
                  advertising_phase < medium: BLE medium: BLUETOOTH >
                  advertising_phase < medium: BLE medium: BLUETOOTH >
                  advertising_phase < medium: BLE medium: BLUETOOTH >
                >)pb")));
}

TEST(AnalyticsRecorderTest, AdvertiserConnectionRequestsWorks) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id_0("endpoint_id_0");
  std::string endpoint_id_1("endpoint_id_1");
  std::string endpoint_id_2("endpoint_id_2");
  std::string endpoint_id_3("endpoint_id_3");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_0);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  analytics_recorder.OnConnectionRequestReceived(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);
  analytics_recorder.OnRemoteEndpointRejected(endpoint_id_1);

  analytics_recorder.OnConnectionRequestReceived(endpoint_id_2);
  analytics_recorder.OnLocalEndpointRejected(endpoint_id_2);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_2);

  analytics_recorder.OnConnectionRequestReceived(endpoint_id_3);
  analytics_recorder.OnLocalEndpointRejected(endpoint_id_3);
  analytics_recorder.OnRemoteEndpointRejected(endpoint_id_3);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    received_connection_request <
                      request_delay_millis: 0
                      local_response: ACCEPTED
                      remote_response: ACCEPTED
                    >
                    received_connection_request <
                      local_response: ACCEPTED
                      remote_response: REJECTED
                    >
                    received_connection_request <
                      local_response: REJECTED
                      remote_response: ACCEPTED
                    >
                    received_connection_request <
                      local_response: REJECTED
                      remote_response: REJECTED
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, DiscoveryConnectionRequestsWorks) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id_0("endpoint_id_0");
  std::string endpoint_id_1("endpoint_id_1");
  std::string endpoint_id_2("endpoint_id_2");
  std::string endpoint_id_3("endpoint_id_3");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(strategy, mediums);

  analytics_recorder.OnConnectionRequestSent(endpoint_id_0);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  analytics_recorder.OnConnectionRequestSent(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);
  analytics_recorder.OnRemoteEndpointRejected(endpoint_id_1);

  analytics_recorder.OnConnectionRequestSent(endpoint_id_2);
  analytics_recorder.OnLocalEndpointRejected(endpoint_id_2);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_2);

  analytics_recorder.OnConnectionRequestSent(endpoint_id_3);
  analytics_recorder.OnLocalEndpointRejected(endpoint_id_3);
  analytics_recorder.OnRemoteEndpointRejected(endpoint_id_3);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: DISCOVERER
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    sent_connection_request <
                      local_response: ACCEPTED
                      remote_response: ACCEPTED
                    >
                    sent_connection_request <
                      local_response: ACCEPTED
                      remote_response: REJECTED
                    >
                    sent_connection_request <
                      local_response: REJECTED
                      remote_response: ACCEPTED
                    >
                    sent_connection_request <
                      local_response: REJECTED
                      remote_response: REJECTED
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest,
     AdvertiserUnfinishedConnectionRequestsIncludedAsIgnored) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id_0("endpoint_id_0");
  std::string endpoint_id_1("endpoint_id_1");
  std::string endpoint_id_2("endpoint_id_2");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  // Ignored by local.
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  // Ignored by remote.
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);

  // Ignored by both.
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_2);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    received_connection_request <
                      local_response: IGNORED
                      remote_response: ACCEPTED
                    >
                    received_connection_request <
                      local_response: ACCEPTED
                      remote_response: IGNORED
                    >
                    received_connection_request <
                      local_response: IGNORED
                      remote_response: IGNORED
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest,
     DiscovererUnfinishedConnectionRequestsIncludedAsIgnored) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id_0("endpoint_id_0");
  std::string endpoint_id_1("endpoint_id_1");
  std::string endpoint_id_2("endpoint_id_2");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(strategy, mediums);

  // Ignored by local.
  analytics_recorder.OnConnectionRequestSent(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  // Ignored by remote.
  analytics_recorder.OnConnectionRequestSent(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);

  // Ignored by both.
  analytics_recorder.OnConnectionRequestSent(endpoint_id_2);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: DISCOVERER
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    sent_connection_request <
                      local_response: IGNORED
                      remote_response: ACCEPTED
                    >
                    sent_connection_request <
                      local_response: ACCEPTED
                      remote_response: IGNORED
                    >
                    sent_connection_request <
                      local_response: IGNORED
                      remote_response: IGNORED
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, SuccessfulIncomingConnectionAttempt) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id("endpoint_id");
  std::string connection_token("");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder{&event_logger};

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnIncomingConnectionAttempt(
      INITIAL, BLUETOOTH, RESULT_SUCCESS, absl::Duration{}, connection_token);
  analytics_recorder.OnStopAdvertising();

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase < medium: BLE medium: BLUETOOTH >
                  connection_attempt <
                    type: INITIAL
                    direction: INCOMING
                    medium: BLUETOOTH
                    attempt_result: RESULT_SUCCESS
                    connection_token: ""
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest,
     FailedConnectionAttemptUpdatesConnectionRequestNotSent) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id("endpoint_id");
  std::string connection_token("");

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnConnectionRequestSent(endpoint_id);
  analytics_recorder.OnOutgoingConnectionAttempt(
      endpoint_id, INITIAL, BLUETOOTH, RESULT_ERROR, absl::Duration{},
      connection_token);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: DISCOVERER
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    sent_connection_request <
                      local_response: NOT_SENT
                      remote_response: NOT_SENT
                    >
                  >
                  connection_attempt <
                    type: INITIAL
                    direction: OUTGOING
                    medium: BLUETOOTH
                    attempt_result: RESULT_ERROR
                    connection_token: ""
                  >
                >)pb")));
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
}  // namespace location
