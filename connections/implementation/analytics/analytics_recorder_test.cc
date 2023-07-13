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

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/proto2/contrib/parse_proto/parse_text_proto.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/analytics/event_logger.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/error_code_params.h"
#include "internal/platform/error_code_recorder.h"
#include "internal/platform/exception.h"
#include "internal/proto/analytics/connections_log.proto.h"
#include "proto/connections_enums.proto.h"
#include "third_party/protobuf/message_lite.h"

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
using ::location::nearby::proto::connections::START_CLIENT_SESSION;
using ::location::nearby::proto::connections::START_STRATEGY_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_STRATEGY_SESSION;
using ::location::nearby::proto::connections::SUCCESS;
using ::location::nearby::proto::connections::UPGRADED;
using ::location::nearby::proto::connections::WEB_RTC;
using ::location::nearby::proto::connections::WIFI_LAN;
using ::location::nearby::proto::connections::WIFI_LAN_MEDIUM_ERROR;
using ::location::nearby::proto::connections::WIFI_LAN_SOCKET_CREATION;
using ::nearby::analytics::EventLogger;
using ::proto2::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::Contains;
using ::protobuf_matchers::EqualsProto;
using ::testing::Not;
using ::testing::proto::Partially;

constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);

class FakeEventLogger : public EventLogger {
 public:
  explicit FakeEventLogger(CountDownLatch& client_session_done_latch)
      : client_session_done_latch_(client_session_done_latch) {}

  FakeEventLogger(CountDownLatch& client_session_done_latch,
                  CountDownLatch* start_client_session_done_latch_ptr)
      : client_session_done_latch_(client_session_done_latch),
        start_client_session_done_latch_ptr_(
            start_client_session_done_latch_ptr) {}

  void Log(const ::google::protobuf::MessageLite& message) override {
    auto connections_log = dynamic_cast<const ConnectionsLog*>(&message);
    if (connections_log == nullptr) {
      return;
    }

    EventType event_type = connections_log->event_type();
    logged_event_types_.push_back(event_type);
    if (event_type == CLIENT_SESSION) {
      logged_client_session_count_++;
      logged_client_session_ = connections_log->client_session();
    }
    if (event_type == ERROR_CODE) {
      error_code_ = connections_log->error_code();
    }
    if (event_type == STOP_CLIENT_SESSION) {
      client_session_done_latch_.CountDown();
    }
    if (start_client_session_done_latch_ptr_ != nullptr &&
        event_type == START_CLIENT_SESSION) {
      start_client_session_done_latch_ptr_->CountDown();
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

  void SetClientSessionDoneLatch(
      const CountDownLatch& client_session_done_latch) {
    client_session_done_latch_ = client_session_done_latch;
  }

  void SetStartClientSessionDoneLatchPtr(
      CountDownLatch* start_client_session_done_latch_ptr) {
    start_client_session_done_latch_ptr_ = start_client_session_done_latch_ptr;
  }

 private:
  int logged_client_session_count_ = 0;
  CountDownLatch& client_session_done_latch_;
  CountDownLatch* start_client_session_done_latch_ptr_ = nullptr;
  ConnectionsLog::ClientSession logged_client_session_;
  ConnectionsLog::ErrorCode error_code_;
  std::vector<EventType> logged_event_types_;
};

// Test if session_was_logged_ is reset by checking if LogSession can take
// effect again or not.
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

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(strategy, /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnStopAdvertising();
  analytics_recorder.OnStartAdvertising(strategy, /*mediums=*/{BLUETOOTH});

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, SetFieldsCorrectlyForNestedDiscoveryCalls) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(
      strategy, /*mediums=*/{BLE, BLUETOOTH},
      /*is_extended_advertisement_supported=*/true,
      /*connected_ap_frequency=*/1, /*is_nfc_available=*/false);
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnEndpointFound(BLUETOOTH);
  analytics_recorder.OnEndpointFound(BLE);
  analytics_recorder.OnStartDiscovery(
      strategy, /*mediums=*/{BLUETOOTH},
      /*is_extended_advertisement_supported=*/true,
      /*connected_ap_frequency=*/1, /*is_nfc_available=*/false);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, AdvertiserConnectionRequestsWorks) {
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string endpoint_id_3 = "endpoint_id_3";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, DiscoveryConnectionRequestsWorks) {
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string endpoint_id_3 = "endpoint_id_3";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLE, BLUETOOTH});

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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest,
     AdvertiserUnfinishedConnectionRequestsIncludedAsIgnored) {
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest,
     DiscovererUnfinishedConnectionRequestsIncludedAsIgnored) {
  std::string endpoint_id_0 = "endpoint_id_0";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLE, BLUETOOTH});

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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, SuccessfulIncomingConnectionAttempt) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnIncomingConnectionAttempt(
      INITIAL, BLUETOOTH, RESULT_SUCCESS, absl::Duration{},
      /*connection_token=*/"", nullptr);
  analytics_recorder.OnStopAdvertising();

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest,
     FailedConnectionAttemptUpdatesConnectionRequestNotSent) {
  std::string endpoint_id = "endpoint_id";

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
  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnConnectionRequestSent(endpoint_id);
  analytics_recorder.OnOutgoingConnectionAttempt(
      endpoint_id, INITIAL, BLUETOOTH, RESULT_ERROR, absl::Duration{},
      /*connection_token=*/"", connections_attempt_metadata_params.get());

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, UnfinishedEstablishedConnectionsAddedAsUnfinished) {
  std::string endpoint_id = "endpoint_id";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnConnectionEstablished(endpoint_id, BLUETOOTH,
                                             connection_token);
  analytics_recorder.OnConnectionClosed(endpoint_id, BLUETOOTH, UPGRADED);
  analytics_recorder.OnConnectionEstablished(endpoint_id, WIFI_LAN,
                                             connection_token);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, OutgoingPayloadUpgraded) {
  std::string endpoint_id = "endpoint_id";
  std::int64_t payload_id = 123456789;
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, UpgradeAttemptWorks) {
  std::string endpoint_id = "endpoint_id";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});

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

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, StartListeningForIncomingConnectionsWorks) {
  std::string endpoint_id = "endpoint_id";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartedIncomingConnectionListening(
      connections::Strategy::kP2pStar);

  analytics_recorder.OnBandwidthUpgradeStarted(endpoint_id, BLE, WIFI_LAN,
                                               INCOMING, connection_token);

  analytics_recorder.OnBandwidthUpgradeStarted(
      endpoint_id_1, BLUETOOTH, WIFI_LAN, INCOMING, connection_token);
  // Error to upgrade.
  analytics_recorder.OnBandwidthUpgradeError(endpoint_id, WIFI_LAN_MEDIUM_ERROR,
                                             WIFI_LAN_SOCKET_CREATION);
  // Success to upgrade.
  analytics_recorder.OnBandwidthUpgradeSuccess(endpoint_id_1);

  analytics_recorder.LogSession();
  // ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
        strategy_session <
          strategy: P2P_STAR
          role: ADVERTISER
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
        >)pb");

  analytics_recorder.Sync();
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectly) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{WEB_RTC});

  ErrorCodeParams error_code_params = ErrorCodeRecorder::BuildErrorCodeParams(
      WEB_RTC, DISCONNECT, DISCONNECT_NETWORK_FAILED,
      TACHYON_SEND_MESSAGE_STATUS_EXCEPTION, "", "connection_token");
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ErrorCode error_code_proto = ParseTextProtoOrDie(R"pb(
    medium: WEB_RTC
    event: DISCONNECT
    description: TACHYON_SEND_MESSAGE_STATUS_EXCEPTION
    disconnect_error: DISCONNECT_NETWORK_FAILED
    connection_token: "connection_token"
  )pb");

  EXPECT_THAT(event_logger.GetErrorCode(),
              Partially(EqualsProto(error_code_proto)));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectlyForUnknownDescription) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLUETOOTH});

  ErrorCodeParams error_code_params;
  // Skip setting error_code_params.description
  error_code_params.medium = BLUETOOTH;
  error_code_params.event = START_DISCOVERING;
  error_code_params.start_discovering_error = START_EXTENDED_DISCOVERING_FAILED;
  error_code_params.connection_token = "connection_token";
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ErrorCode error_code_proto = ParseTextProtoOrDie(R"pb(
    medium: BLUETOOTH
    event: START_DISCOVERING
    description: UNKNOWN
    start_discovering_error: START_EXTENDED_DISCOVERING_FAILED
    connection_token: "connection_token"
  )pb");

  EXPECT_THAT(event_logger.GetErrorCode(),
              Partially(EqualsProto(error_code_proto)));
}

TEST(AnalyticsRecorderTest, SetErrorCodeFieldsCorrectlyForCommonError) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);
  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLUETOOTH});

  ErrorCodeParams error_code_params = ErrorCodeRecorder::BuildErrorCodeParams(
      BLUETOOTH, START_DISCOVERING, INVALID_PARAMETER,
      NULL_BLUETOOTH_DEVICE_NAME, "", "connection_token");
  analytics_recorder.OnErrorCode(error_code_params);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ErrorCode error_code_proto = ParseTextProtoOrDie(R"pb(
    medium: BLUETOOTH
    event: START_DISCOVERING
    description: NULL_BLUETOOTH_DEVICE_NAME
    common_error: INVALID_PARAMETER
    connection_token: "connection_token"
  )pb");

  EXPECT_THAT(event_logger.GetErrorCode(),
              Partially(EqualsProto(error_code_proto)));
}

TEST(AnalyticsRecorderTest, CheckIfSessionWasLogged) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  // LogSession to count down client_session_done_latch.
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_TRUE(analytics_recorder.IsSessionLogged());
}

TEST(AnalyticsRecorderTest, ConstructAnalyticsRecorder) {
  CountDownLatch client_session_done_latch(0);
  CountDownLatch start_client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch,
                               &start_client_session_done_latch);

  // Call the constructor to count down the session_done_latch.
  AnalyticsRecorder analytics_recorder(&event_logger);
  ASSERT_TRUE(start_client_session_done_latch.Await(kDefaultTimeout).result());

  std::vector<EventType> event_types = event_logger.GetLoggedEventTypes();
  EXPECT_EQ(event_types.size(), 1);
  EXPECT_THAT(event_types, Contains(START_CLIENT_SESSION).Times(1));
}

TEST(AnalyticsRecorderTest,
     StartClientSessionOnlyLoggedOnceWorksAfterAnalyticsRecorderIsConstructed) {
  CountDownLatch client_session_done_latch(0);
  CountDownLatch start_client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch,
                               &start_client_session_done_latch);

  // Call the constructor to count down the start_client_session_done_latch.
  AnalyticsRecorder analytics_recorder(&event_logger);
  ASSERT_TRUE(start_client_session_done_latch.Await(kDefaultTimeout).result());

  // Log start client session once.
  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_CLIENT_SESSION).Times(1));

  // Reset the start_client_session_done_latch. However, LogStartSession cannot
  // count down the start_client_session_done_latch.
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_FALSE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // No more start client session was logged.
  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_CLIENT_SESSION).Times(1));
}

TEST(AnalyticsRecorderTest,
     CanLogStartClientSessionOnceAgainAfterSessionWasLogged) {
  CountDownLatch client_session_done_latch(0);
  CountDownLatch start_client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch,
                               &start_client_session_done_latch);

  // Call the constructor to count down the start_client_session_done_latch.
  AnalyticsRecorder analytics_recorder(&event_logger);
  ASSERT_TRUE(start_client_session_done_latch.Await(kDefaultTimeout).result());

  // Log start client session once.
  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_CLIENT_SESSION).Times(1));

  // Reset the client_session_done_latch. Call LogSession to count down the
  // client_session_done_latch.
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  // Reset the start_client_session_done_latch. Call LogStartSession to count
  // down the start_client_session_done_latch.
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  analytics_recorder.LogStartSession();
  analytics_recorder.LogStartSession();
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // Can log start client session once again.
  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_CLIENT_SESSION).Times(2));
}

TEST(AnalyticsRecorderTest,
     ClearcIncomingConnectionRequestsAfterSessionWasLogged) {
  std::string endpoint_id_0 = "endpoint_id_0";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_0);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  // LogSession
  analytics_recorder.LogSession();  // call ResetClientSessionLoggingResouces
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
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
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  std::string endpoint_id_1 = "endpoint_id_1";
  analytics_recorder.OnConnectionRequestReceived(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_1);

  analytics_recorder.LogSession();
  ASSERT_TRUE(new_client_session_done_latch.Await(kDefaultTimeout).result());

  // - if the current_strategy_session_ and current_advertising_phase_ are not
  // reset, the duplicate advertising_phase (with the additional
  // received_connection_request) will append to the strategy_session)
  ConnectionsLog::ClientSession strategy_session_proto2 = ParseTextProtoOrDie(
      R"pb(
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
          >
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
              remote_response: ACCEPTED
            >
          >
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

TEST(AnalyticsRecorderTest,
     ClearcOutgoingConnectionRequestsAfterSessionWasLogged) {
  std::string endpoint_id_0 = "endpoint_id_0";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(connections::Strategy::kP2pStar,
                                      /*mediums=*/{BLE, BLUETOOTH});

  analytics_recorder.OnConnectionRequestSent(endpoint_id_0);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_0);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_0);

  // LogSession
  analytics_recorder.LogSession();  // call ResetClientSessionLoggingResouces
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
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
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  std::string endpoint_id_1 = "endpoint_id_1";
  analytics_recorder.OnConnectionRequestSent(endpoint_id_1);
  analytics_recorder.OnLocalEndpointAccepted(endpoint_id_1);
  analytics_recorder.OnRemoteEndpointAccepted(endpoint_id_1);

  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  // - if the current_strategy_session_ and current_discovery_phase_ are
  // not reset, the duplicate discovery_phase (with the additional
  // sent_connection_request) will append to the strategy_session)
  ConnectionsLog::ClientSession strategy_session_proto2 =
      ParseTextProtoOrDie(R"pb(
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
          >
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
              remote_response: ACCEPTED
            >
          >
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

TEST(AnalyticsRecorderTest, ClearcActiveConnectionsAfterSessionWasLogged) {
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

  // LogSession
  analytics_recorder.LogSession();  // call ResetClientSessionLoggingResouces
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());
  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
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
            disconnection_reason: UNFINISHED
            connection_token: "connection_token"
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  // - if the current_strategy_session_ and advertising_phase_ are not
  // reset, the duplicate advertising_phase_ (with the additional
  // will append to the strategy_session), and the active connection (i.e.
  // established_connection) will stay there.
  ConnectionsLog::ClientSession strategy_session_proto2 =
      ParseTextProtoOrDie(R"pb(
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
            disconnection_reason: UNFINISHED
            connection_token: "connection_token"
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

TEST(AnalyticsRecorderTest,
     ClearBandwidthUpgradeAttemptsAfterSessionWasLogged) {
  std::string endpoint_id = "endpoint_id";
  std::string endpoint_id_1 = "endpoint_id_1";
  std::string endpoint_id_2 = "endpoint_id_2";
  std::string connection_token = "connection_token";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});

  analytics_recorder.OnBandwidthUpgradeStarted(endpoint_id, BLE, WIFI_LAN,
                                               INCOMING, connection_token);

  analytics_recorder.OnBandwidthUpgradeStarted(
      endpoint_id_1, BLUETOOTH, WIFI_LAN, INCOMING, connection_token);
  // - Error to upgrade.
  analytics_recorder.OnBandwidthUpgradeError(endpoint_id, WIFI_LAN_MEDIUM_ERROR,
                                             WIFI_LAN_SOCKET_CREATION);
  // - Success to upgrade.
  analytics_recorder.OnBandwidthUpgradeSuccess(endpoint_id_1);

  // - Upgrade is unfinished.
  analytics_recorder.OnBandwidthUpgradeStarted(
      endpoint_id_2, BLUETOOTH, WIFI_LAN, INCOMING, connection_token);

  // LogSession
  analytics_recorder.LogSession();  // call ResetClientSessionLoggingResouces
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  // - if the current_strategy_session_ and advertising_phase_ are not
  // reset, the duplicate advertising_phase_, and the upgrade_attempts (i.e.
  // bandwidth_upgrade_attempts_) will stay there.
  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto2 =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

// Test if current_strategy_ is reset by checking if the same strategy would
// be logged for different client sessions or not. If yes, it should be logged.
// Otherwise, not.
TEST(AnalyticsRecorderTest,
     CanLogSeparateStartStrategySessionForSameStrategyAfterSessionWasLogged) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLUETOOTH});
  analytics_recorder.OnStopAdvertising();

  // LogSession
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  //// The same strategy session shouldn't be logged again with the same client
  //// session.
  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_STRATEGY_SESSION).Times(1));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.OnStartAdvertising(strategy, /*mediums=*/{BLUETOOTH});
  analytics_recorder.OnStopAdvertising();

  analytics_recorder.LogSession();
  ASSERT_TRUE(new_client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedEventTypes(),
              Contains(START_STRATEGY_SESSION).Times(2));
}

// Test if current_strategy_session_ is reset. If not, the same strategy session
// proto will be logged.
TEST(AnalyticsRecorderTest,
     NotLogSameStrategySessionProtoAfterSessionWasLogged) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  //// Via OnStartAdvertising, current_strategy_session_is set in
  //// UpdateStrategySessionLocked.
  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnStopAdvertising();

  // LogSession
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  // - if current_strategy_session_ is reset, the same strategy_session_proto
  // will be logged.
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(new_client_session_done_latch.Await(kDefaultTimeout).result());

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto))));
}

// Test if current_advertising_phase_ is reset.
TEST(AnalyticsRecorderTest,
     NotLogDuplicateAdvertisingPhaseAfterSessionWasLogged) {
  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartAdvertising(
      connections::Strategy::kP2pStar,
      /*mediums=*/{BLUETOOTH});  // set current_advertising_phase_
  analytics_recorder.OnStopAdvertising();

  // LogSession
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
        strategy_session <
          strategy: P2P_STAR
          role: ADVERTISER
          advertising_phase <
            medium: BLUETOOTH
            advertising_metadata <
              supports_extended_ble_advertisements: false
              connected_ap_frequency: 0
              supports_nfc_technology: false
            >
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  // - if the current_strategy_session_ and current_advertising_phase_ are
  // not reset, the same strategy_session with two same advertising_phase will
  // be logged.
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto2 =
      ParseTextProtoOrDie(R"pb(
        strategy_session <
          strategy: P2P_STAR
          role: ADVERTISER
          advertising_phase <
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
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

// Test if current_discovery_phase_ is reset.
TEST(AnalyticsRecorderTest,
     NotLogDuplicateDiscoveryPhaseAfterSessionWasLogged) {
  connections::Strategy strategy = connections::Strategy::kP2pStar;

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  analytics_recorder.OnStartDiscovery(
      strategy, {BLUETOOTH}, /*is_extended_advertisement_supported=*/true,
      /*connected_ap_frequency=*/1,
      /*is_nfc_available=*/false);  // set current_discovery_phase_
  analytics_recorder.OnStopDiscovery();
  analytics_recorder.OnEndpointFound(BLUETOOTH);

  // LogSession
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto1 =
      ParseTextProtoOrDie(R"pb(
        strategy_session <
          strategy: P2P_STAR
          role: DISCOVERER
          discovery_phase <
            medium: BLUETOOTH
            discovery_metadata <
              supports_extended_ble_advertisements: true
              connected_ap_frequency: 1
              supports_nfc_technology: false
            >
          >
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto1)));

  // LogStartSession
  CountDownLatch new_start_client_session_done_latch(1);
  event_logger.SetStartClientSessionDoneLatchPtr(
      &new_start_client_session_done_latch);
  analytics_recorder.LogStartSession();
  ASSERT_TRUE(
      new_start_client_session_done_latch.Await(kDefaultTimeout).result());

  // LogSession again
  // - if the current_strategy_session_ and current_discovery_phase_ are not
  // reset, the same strategy_session with two same discovery_phase will be
  // logged.
  CountDownLatch new_client_session_done_latch(1);
  event_logger.SetClientSessionDoneLatch(new_client_session_done_latch);
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto2 =
      ParseTextProtoOrDie(R"pb(
        strategy_session <
          strategy: P2P_STAR
          role: DISCOVERER
          discovery_phase <
            medium: BLUETOOTH
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
        >)pb");
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Not(Partially(EqualsProto(strategy_session_proto2))));
}

TEST(AnalyticsRecorderOnConnectionClosedTest,
     NotAddNewConnectionWithoutCallingOnStartAdvertising) {
  std::string endpoint_id = "endpoint_id";

  CountDownLatch client_session_done_latch(1);
  FakeEventLogger event_logger(client_session_done_latch);
  AnalyticsRecorder analytics_recorder(&event_logger);

  // via OnStartAdvertising, current_strategy_session_ is set in
  // UpdateStrategySessionLocked.
  analytics_recorder.OnStartAdvertising(connections::Strategy::kP2pStar,
                                        /*mediums=*/{BLE, BLUETOOTH});
  analytics_recorder.OnStopAdvertising();

  // LogSession
  analytics_recorder.LogSession();
  ASSERT_TRUE(client_session_done_latch.Await(kDefaultTimeout).result());

  ConnectionsLog::ClientSession strategy_session_proto =
      ParseTextProtoOrDie(R"pb(
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
        >)pb");

  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));

  // Without calling OnStartAdvertising won't create new
  // current_strategy_session_.
  analytics_recorder.OnConnectionEstablished(endpoint_id, BLUETOOTH,
                                             /*connection_token=*/"");
  analytics_recorder.OnConnectionClosed(endpoint_id, BLUETOOTH, UPGRADED);

  analytics_recorder.LogSession();

  // The proto won't change.
  EXPECT_THAT(event_logger.GetLoggedClientSession(),
              Partially(EqualsProto(strategy_session_proto)));
}

}  // namespace
}  // namespace analytics
}  // namespace nearby
