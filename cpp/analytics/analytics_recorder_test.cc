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

using ::location::nearby::proto::connections::ACCEPTED;
using ::location::nearby::proto::connections::BLE;
using ::location::nearby::proto::connections::BLUETOOTH;
using ::location::nearby::proto::connections::CLIENT_SESSION;
using ::location::nearby::proto::connections::ConnectionRequestResponse;
using ::location::nearby::proto::connections::EventType;
using ::location::nearby::proto::connections::IGNORED;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::REJECTED;
using ::location::nearby::proto::connections::START_STRATEGY_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_STRATEGY_SESSION;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAreArray;

using ::location::nearby::analytics::proto::ConnectionsLog;
using ClientSession =
    ::location::nearby::analytics::proto::ConnectionsLog::ClientSession;
using StrategySession =
    ::location::nearby::analytics::proto::ConnectionsLog::StrategySession;
using AdvertisingPhase =
    ::location::nearby::analytics::proto::ConnectionsLog::AdvertisingPhase;
using DiscoveryPhase =
    ::location::nearby::analytics::proto::ConnectionsLog::DiscoveryPhase;
using DiscoveredEndpoint =
    ::location::nearby::analytics::proto::ConnectionsLog::DiscoveredEndpoint;
using ConnectionRequest =
    ::location::nearby::analytics::proto::ConnectionsLog::ConnectionRequest;

constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);

class FakeEventLogger : public EventLogger {
 public:
  explicit FakeEventLogger(CountDownLatch& client_session_done_latch)
      : client_session_done_latch_(client_session_done_latch) {}

  void Log(const ConnectionsLog& connections_log) override {
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

  const ClientSession& GetLoggedClientSession() {
    return logged_client_session_;
  }

  std::vector<EventType> GetLoggedEventTypes() { return logged_event_types_; }

 private:
  int logged_client_session_count_ = 0;
  CountDownLatch& client_session_done_latch_;
  ClientSession logged_client_session_;
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  ASSERT_EQ(client_session.strategy_session_size(), 1);
  ASSERT_EQ(client_session.strategy_session(0).advertising_phase_size(), 2);
  EXPECT_THAT(client_session.strategy_session(0).advertising_phase(0).medium(),
              UnorderedElementsAreArray(mediums));
  EXPECT_THAT(client_session.strategy_session(0).advertising_phase(1).medium(),
              ElementsAre(BLUETOOTH));
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  EXPECT_EQ(client_session.strategy_session_size(), 1);
  EXPECT_EQ(client_session.strategy_session(0).discovery_phase_size(), 2);
  EXPECT_THAT(client_session.strategy_session(0).discovery_phase(0).medium(),
              UnorderedElementsAreArray(mediums));
  EXPECT_EQ(client_session.strategy_session(0)
                .discovery_phase(0)
                .discovered_endpoint_size(),
            2);
  EXPECT_EQ(client_session.strategy_session(0)
                .discovery_phase(0)
                .discovered_endpoint(0)
                .medium(),
            BLUETOOTH);
  EXPECT_EQ(client_session.strategy_session(0)
                .discovery_phase(0)
                .discovered_endpoint(1)
                .medium(),
            BLE);
  EXPECT_THAT(client_session.strategy_session(0).discovery_phase(1).medium(),
              ElementsAre(BLUETOOTH));
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  std::vector<EventType> event_types = event_logger.GetLoggedEventTypes();
  EXPECT_THAT(event_types, Contains(START_STRATEGY_SESSION).Times(1));
  EXPECT_THAT(event_types, Contains(STOP_STRATEGY_SESSION).Times(1));
  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  EXPECT_EQ(client_session.strategy_session_size(), 1);
  EXPECT_EQ(client_session.strategy_session(0).advertising_phase_size(), 3);
  EXPECT_EQ(client_session.strategy_session(0).discovery_phase_size(), 3);
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  StrategySession strategy_session = client_session.strategy_session(0);
  EXPECT_EQ(strategy_session.advertising_phase_size(), 1);
  EXPECT_EQ(
      strategy_session.advertising_phase(0).received_connection_request_size(),
      4);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(0)
                .local_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(0)
                .remote_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(1)
                .local_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(1)
                .remote_response(),
            REJECTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(2)
                .local_response(),
            REJECTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(2)
                .remote_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(3)
                .local_response(),
            REJECTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(3)
                .remote_response(),
            REJECTED);
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  StrategySession strategy_session = client_session.strategy_session(0);
  EXPECT_EQ(strategy_session.discovery_phase_size(), 1);
  EXPECT_EQ(strategy_session.discovery_phase(0).sent_connection_request_size(),
            4);
  auto& sent_connection_request_0 =
      strategy_session.discovery_phase(0).sent_connection_request(0);
  EXPECT_EQ(sent_connection_request_0.local_response(), ACCEPTED);
  EXPECT_EQ(sent_connection_request_0.remote_response(), ACCEPTED);
  auto& sent_connection_request_1 =
      strategy_session.discovery_phase(0).sent_connection_request(1);
  EXPECT_EQ(sent_connection_request_1.local_response(), ACCEPTED);
  EXPECT_EQ(sent_connection_request_1.remote_response(), REJECTED);
  auto& sent_connection_request_2 =
      strategy_session.discovery_phase(0).sent_connection_request(2);
  EXPECT_EQ(sent_connection_request_2.local_response(), REJECTED);
  EXPECT_EQ(sent_connection_request_2.remote_response(), ACCEPTED);
  auto& sent_connection_request_3 =
      strategy_session.discovery_phase(0).sent_connection_request(3);
  EXPECT_EQ(sent_connection_request_3.local_response(), REJECTED);
  EXPECT_EQ(sent_connection_request_3.remote_response(), REJECTED);
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  StrategySession strategy_session = client_session.strategy_session(0);
  EXPECT_EQ(strategy_session.advertising_phase_size(), 1);
  EXPECT_EQ(
      strategy_session.advertising_phase(0).received_connection_request_size(),
      3);

  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(0)
                .local_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(0)
                .remote_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(1)
                .local_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(1)
                .remote_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(2)
                .local_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.advertising_phase(0)
                .received_connection_request(2)
                .remote_response(),
            IGNORED);
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
  EXPECT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  const ClientSession& client_session = event_logger.GetLoggedClientSession();
  StrategySession strategy_session = client_session.strategy_session(0);
  EXPECT_EQ(strategy_session.discovery_phase_size(), 1);
  EXPECT_EQ(strategy_session.discovery_phase(0).sent_connection_request_size(),
            3);

  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(0)
                .local_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(0)
                .remote_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(1)
                .local_response(),
            ACCEPTED);
  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(1)
                .remote_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(2)
                .local_response(),
            IGNORED);
  EXPECT_EQ(strategy_session.discovery_phase(0)
                .sent_connection_request(2)
                .remote_response(),
            IGNORED);
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
}  // namespace location
