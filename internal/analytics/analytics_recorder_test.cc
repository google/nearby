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

#include "internal/analytics/analytics_recorder.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/analytics/connection_attempt_metadata_params.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace analytics {
namespace {

using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::errorcode::proto::DISCONNECT;
using ::location::nearby::errorcode::proto::DISCONNECT_NETWORK_FAILED;
using ::location::nearby::errorcode::proto::INVALID_PARAMETER;
using ::location::nearby::errorcode::proto::NULL_BLUETOOTH_DEVICE_NAME;
using ::location::nearby::errorcode::proto::START_DISCOVERING;
using ::location::nearby::errorcode::proto::START_EXTENDED_DISCOVERING_FAILED;
using ::location::nearby::errorcode::proto::
    TACHYON_SEND_MESSAGE_STATUS_EXCEPTION;
using ::location::nearby::proto::connections::BLE;
using ::location::nearby::proto::connections::BLUETOOTH;
using ::location::nearby::proto::connections::CLIENT_SESSION;
using ::location::nearby::proto::connections::ERROR_CODE;
using ::location::nearby::proto::connections::EventType;
using ::location::nearby::proto::connections::INCOMING;
using ::location::nearby::proto::connections::INITIAL;
using ::location::nearby::proto::connections::LOCAL_DISCONNECTION;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::RESULT_ERROR;
using ::location::nearby::proto::connections::RESULT_SUCCESS;
using ::location::nearby::proto::connections::START_STRATEGY_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_STRATEGY_SESSION;
using ::location::nearby::proto::connections::SUCCESS;
using ::location::nearby::proto::connections::UPGRADED;
using ::location::nearby::proto::connections::WEB_RTC;
using ::location::nearby::proto::connections::WIFI_LAN;
using ::location::nearby::proto::connections::WIFI_LAN_MEDIUM_ERROR;
using ::location::nearby::proto::connections::WIFI_LAN_SOCKET_CREATION;

using ::testing::Contains;
using ::protobuf_matchers::EqualsProto;
using ::testing::proto::Partially;

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
    if (event_type == ERROR_CODE) {
      error_code_ = connections_log.error_code();
    }
    if (event_type == STOP_CLIENT_SESSION) {
      client_session_done_latch_.CountDown();
    }
  }

  int GetLoggedClientSessionCount() const {
    return logged_client_session_count_;
  }

  const ConnectionsLog::ClientSession& GetLoggedClientSession() {
    return logged_client_session_;
  }

  const ConnectionsLog::ErrorCode& GetErrorCode() { return error_code_; }

  std::vector<EventType> GetLoggedEventTypes() { return logged_event_types_; }

 private:
  int logged_client_session_count_ = 0;
  CountDownLatch& client_session_done_latch_;
  ConnectionsLog::ClientSession logged_client_session_;
  ConnectionsLog::ErrorCode error_code_;
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
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  advertising_phase <
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, SetFieldsCorrectlyForNestedDiscoveryCalls) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(strategy, mediums, true, 1, false);
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnEndpointFound(BLUETOOTH);
  analytics_recorder.OnEndpointFound(BLE);
  analytics_recorder.OnStartDiscovery(strategy, {BLUETOOTH}, true, 1, false);

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
                    discovery_metadata <
                      supports_extended_ble_advertisements: true
                      connected_ap_frequency: 1
                      supports_nfc_technology: false
                    >
                  >
                  discovery_phase <
                    medium: BLUETOOTH
                    discovery_metadata <
                      supports_extended_ble_advertisements: true
                      connected_ap_frequency: 1
                      supports_nfc_technology: false
                    >
                  >
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
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, AdvertiserConnectionRequestsWorks) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string endpoint_id_3 = "endpoint_id_3";

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
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                    received_connection_request <
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
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string endpoint_id_3 = "endpoint_id_3";

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
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
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
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";

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
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
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
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";

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
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
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
  std::string endpoint_id = "endpoint_id";
  std::string connection_token = "";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder{&event_logger};

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnIncomingConnectionAttempt(
      INITIAL, BLUETOOTH, RESULT_SUCCESS, absl::Duration{}, connection_token,
      nullptr);
  analytics_recorder.OnStopAdvertising();

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  connection_attempt <
                    type: INITIAL
                    direction: INCOMING
                    medium: BLUETOOTH
                    attempt_result: RESULT_SUCCESS
                    connection_token: ""
                    connection_attempt_metadata <
                      technology: CONNECTION_TECHNOLOGY_UNKNOWN_TECHNOLOGY
                      band: CONNECTION_BAND_UNKNOWN_BAND
                      frequency: -1
                      network_operator: ""
                      country_code: ""
                      is_tdls_used: false
                      try_counts: 0
                      wifi_hotspot_status: false
                      max_tx_speed: 0
                      max_rx_speed: 0
                      wifi_channel_width: -1
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest,
     FailedConnectionAttemptUpdatesConnectionRequestNotSent) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id = "endpoint_id";
  std::string connection_token = "";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  auto connections_attempt_metadata_params =
      analytics_recorder.BuildConnectionAttemptMetadataParams(
          ::location::nearby::proto::connections::
              CONNECTION_TECHNOLOGY_HOTSPOT_LOCALONLY,
          ::location::nearby::proto::connections::
              CONNECTION_BAND_WIFI_BAND_6GHZ,
          /*frequency*/ 2400, /*try_count*/ 0, /*network_operator*/ {},
          /*country_code*/ {}, /*is_tdls_used*/ false,
          /*wifi_hotspot_enabled*/ false, /*max_wifi_tx_speed*/ 0,
          /*max_wifi_rx_speed*/ 0, /*channel_width*/ 0);
  analytics_recorder.OnStartDiscovery(strategy, mediums);
  analytics_recorder.OnConnectionRequestSent(endpoint_id);
  analytics_recorder.OnOutgoingConnectionAttempt(
      endpoint_id, INITIAL, BLUETOOTH, RESULT_ERROR, absl::Duration{},
      connection_token, connections_attempt_metadata_params.get());

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: DISCOVERER
                  discovery_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    discovery_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
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
                    connection_attempt_metadata <
                      technology: CONNECTION_TECHNOLOGY_HOTSPOT_LOCALONLY
                      band: CONNECTION_BAND_WIFI_BAND_6GHZ
                      frequency: 2400
                      network_operator: ""
                      country_code: ""
                      is_tdls_used: false
                      try_counts: 0
                      wifi_hotspot_status: false
                      max_tx_speed: 0
                      max_rx_speed: 0
                      wifi_channel_width: 0
                    >
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, UnfinishedEstablishedConnectionsAddedAsUnfinished) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id = "endpoint_id";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnConnectionEstablished(endpoint_id, BLUETOOTH,
                                             connection_token);
  analytics_recorder.OnConnectionClosed(endpoint_id, BLUETOOTH, UPGRADED);
  analytics_recorder.OnConnectionEstablished(endpoint_id, WIFI_LAN,
                                             connection_token);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  established_connection <
                    medium: BLUETOOTH
                    disconnection_reason: UPGRADED
                    connection_token: "connection_token"
                  >
                  established_connection <
                    medium: WIFI_LAN
                    disconnection_reason: UNFINISHED
                    connection_token: "connection_token"
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, OutgoingPayloadUpgraded) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id = "endpoint_id";
  std::int64_t payload_id = 123456789;
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);
  analytics_recorder.OnConnectionEstablished(endpoint_id, BLUETOOTH,
                                             connection_token);
  analytics_recorder.OnOutgoingPayloadStarted(
      {endpoint_id}, payload_id, connections::PayloadType::kFile, 50);
  analytics_recorder.OnPayloadChunkSent(endpoint_id, payload_id, 10);
  analytics_recorder.OnPayloadChunkSent(endpoint_id, payload_id, 10);
  analytics_recorder.OnConnectionClosed(endpoint_id, BLUETOOTH, UPGRADED);
  analytics_recorder.OnConnectionEstablished(endpoint_id, WIFI_LAN,
                                             connection_token);
  analytics_recorder.OnPayloadChunkSent(endpoint_id, payload_id, 10);
  analytics_recorder.OnPayloadChunkSent(endpoint_id, payload_id, 10);
  analytics_recorder.OnPayloadChunkSent(endpoint_id, payload_id, 10);
  analytics_recorder.OnOutgoingPayloadDone(endpoint_id, payload_id, SUCCESS);
  analytics_recorder.OnConnectionClosed(endpoint_id, WIFI_LAN,
                                        LOCAL_DISCONNECTION);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  established_connection <
                    medium: BLUETOOTH
                    sent_payload <
                      type: FILE
                      total_size_bytes: 50
                      num_bytes_transferred: 20
                      num_chunks: 2
                      status: MOVED_TO_NEW_MEDIUM
                    >
                    disconnection_reason: UPGRADED
                    connection_token: "connection_token"
                  >
                  established_connection <
                    medium: WIFI_LAN
                    sent_payload <
                      type: FILE
                      total_size_bytes: 50
                      num_bytes_transferred: 30
                      num_chunks: 3
                      status: SUCCESS
                    >
                    disconnection_reason: LOCAL_DISCONNECTION
                    connection_token: "connection_token"
                  >
                >)pb")));
}

TEST(AnalyticsRecorderTest, UpgradeAttemptWorks) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLE, BLUETOOTH};
  std::string endpoint_id = "endpoint_id";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, mediums);

  analytics_recorder.OnBandwidthUpgradeStarted(endpoint_id, BLE, WIFI_LAN,
                                               INCOMING, connection_token);

  analytics_recorder.OnBandwidthUpgradeStarted(
      endpoint_id_1, BLUETOOTH, WIFI_LAN, INCOMING, connection_token);
  // Error to upgrade.
  analytics_recorder.OnBandwidthUpgradeError(endpoint_id, WIFI_LAN_MEDIUM_ERROR,
                                             WIFI_LAN_SOCKET_CREATION);
  // Success to upgrade.
  analytics_recorder.OnBandwidthUpgradeSuccess(endpoint_id_1);
  // Upgrade is unfinished.
  analytics_recorder.OnBandwidthUpgradeStarted(
      endpoint_id_2, BLUETOOTH, WIFI_LAN, INCOMING, connection_token);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(), Partially(EqualsProto(R"pb(
                strategy_session <
                  strategy: P2P_STAR
                  role: ADVERTISER
                  advertising_phase <
                    medium: BLE
                    medium: BLUETOOTH
                    advertising_metadata <
                      supports_extended_ble_advertisements: false
                      connected_ap_frequency: 0
                      supports_nfc_technology: false
                    >
                  >
                  upgrade_attempt <
                    direction: INCOMING
                    from_medium: BLE
                    to_medium: WIFI_LAN
                    upgrade_result: WIFI_LAN_MEDIUM_ERROR
                    error_stage: WIFI_LAN_SOCKET_CREATION
                    connection_token: "connection_token"
                  >
                  upgrade_attempt <
                    direction: INCOMING
                    from_medium: BLUETOOTH
                    to_medium: WIFI_LAN
                    upgrade_result: UPGRADE_RESULT_SUCCESS
                    error_stage: UPGRADE_SUCCESS
                    connection_token: "connection_token"
                  >
                  upgrade_attempt {
                    direction: INCOMING
                    from_medium: BLUETOOTH
                    to_medium: WIFI_LAN
                    upgrade_result: UNFINISHED_ERROR
                    error_stage: UPGRADE_UNFINISHED
                    connection_token: "connection_token"
                  }
                >)pb")));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectly) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {WEB_RTC};
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(strategy, mediums);

  ErrorCodeParams error_code_params = ErrorCodeRecorder::BuildErrorCodeParams(
      WEB_RTC, DISCONNECT, DISCONNECT_NETWORK_FAILED,
      TACHYON_SEND_MESSAGE_STATUS_EXCEPTION, "", connection_token);
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetErrorCode(), Partially(EqualsProto(R"pb(
                medium: WEB_RTC
                event: DISCONNECT
                description: TACHYON_SEND_MESSAGE_STATUS_EXCEPTION
                disconnect_error: DISCONNECT_NETWORK_FAILED
                connection_token: "connection_token"
              )pb")));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectlyForUnknownDescription) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLUETOOTH};
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(strategy, mediums);

  ErrorCodeParams error_code_params;
  // Skip setting error_code_params.description
  error_code_params.medium = BLUETOOTH;
  error_code_params.event = START_DISCOVERING;
  error_code_params.start_discovering_error = START_EXTENDED_DISCOVERING_FAILED;
  error_code_params.connection_token = connection_token;
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetErrorCode(), Partially(EqualsProto(R"pb(
                medium: BLUETOOTH
                event: START_DISCOVERING
                description: UNKNOWN
                start_discovering_error: START_EXTENDED_DISCOVERING_FAILED
                connection_token: "connection_token"
              )pb")));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectlyForCommonError) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;
  std::vector<Medium> mediums = {BLUETOOTH};
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(strategy, mediums);

  ErrorCodeParams error_code_params = ErrorCodeRecorder::BuildErrorCodeParams(
      BLUETOOTH, START_DISCOVERING, INVALID_PARAMETER,
      NULL_BLUETOOTH_DEVICE_NAME, "", connection_token);
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetErrorCode(), Partially(EqualsProto(R"pb(
                medium: BLUETOOTH
                event: START_DISCOVERING
                description: NULL_BLUETOOTH_DEVICE_NAME
                common_error: INVALID_PARAMETER
                connection_token: "connection_token"
              )pb")));
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
}  // namespace location
