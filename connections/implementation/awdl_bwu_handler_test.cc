// Copyright 2026 Google LLC
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

#include "connections/implementation/awdl_bwu_handler.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/awdl_endpoint_channel.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/awdl.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/strategy.h"
#include "internal/analytics/mock_event_logger.h"
#include "internal/analytics/sharing_log_matchers.h"
#include "internal/platform/awdl.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/mock_input_stream.h"
#include "internal/platform/mock_output_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/proto/analytics/connections_log.pb.h"

namespace nearby {

class MockAwdlSocket : public api::AwdlSocket {
 public:
  MOCK_METHOD(InputStream&, GetInputStream, (), (override));
  MOCK_METHOD(OutputStream&, GetOutputStream, (), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

class MockAwdlServerSocket : public api::AwdlServerSocket {
 public:
  MOCK_METHOD(std::string, GetIPAddress, (), (const, override));
  MOCK_METHOD(int, GetPort, (), (const, override));
  MOCK_METHOD(std::unique_ptr<api::AwdlSocket>, Accept, (), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

class MockAwdlMedium : public api::AwdlMedium {
 public:
  MOCK_METHOD(bool, IsNetworkConnected, (), (const, override));
  MOCK_METHOD(bool, StartAdvertising, (const NsdServiceInfo& nsd_service_info),
              (override));
  MOCK_METHOD(bool, StopAdvertising, (const NsdServiceInfo& nsd_service_info),
              (override));
  MOCK_METHOD(bool, StartDiscovery,
              (const std::string& service_type,
               DiscoveredServiceCallback callback),
              (override));
  MOCK_METHOD(bool, StopDiscovery, (const std::string& service_type),
              (override));
  MOCK_METHOD(std::unique_ptr<api::AwdlSocket>, ConnectToService,
              (const NsdServiceInfo& remote_service_info,
               CancellationFlag* cancellation_flag),
              (override));
  MOCK_METHOD(std::unique_ptr<api::AwdlSocket>, ConnectToService,
              (const NsdServiceInfo& remote_service_info,
               const api::PskInfo& psk_info,
               CancellationFlag* cancellation_flag),
              (override));
  MOCK_METHOD(std::unique_ptr<api::AwdlServerSocket>, ListenForService,
              (int port), (override));
  MOCK_METHOD(std::unique_ptr<api::AwdlServerSocket>, ListenForService,
              (const api::PskInfo& psk_info, int port), (override));
  MOCK_METHOD((std::optional<std::pair<std::int32_t, std::int32_t>>),
              GetDynamicPortRange, (), (override));
};

MockAwdlMedium* awdl_medium_mock = nullptr;

namespace connections {
namespace {

using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::EventType;
using ::location::nearby::proto::connections::OperationResultCode;
using ::nearby::analytics::HasEventType;
using ::testing::_;
using ::testing::ByMove;
using ::protobuf_matchers::EqualsProto;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

constexpr absl::string_view kServiceId{"service_id"};
constexpr absl::string_view kEndpointId{"endpoint_id"};
constexpr absl::string_view kServiceName{"awdl_srv"};
constexpr absl::string_view kServiceType{"_awdl._tcp"};
constexpr absl::string_view kPassword{"password123"};
constexpr absl::string_view kChannelName{"channel_name"};

class AwdlBwuHandlerTest : public ::testing::Test {
 protected:
  AwdlBwuHandlerTest()
      : handler_(mediums_, incoming_connection_callback_.AsStdFunction()) {}

  void SetUp() override {
    // By default, network is connected.
    ON_CALL(*awdl_medium_mock, IsNetworkConnected())
        .WillByDefault(Return(true));
  }

  Mediums mediums_;
  MockFunction<void(ClientProxy*,
                    std::unique_ptr<BwuHandler::IncomingSocketConnection>)>
      incoming_connection_callback_;
  AwdlBwuHandler handler_;
  nearby::analytics::MockEventLogger mock_event_logger_;
  MockInputStream mock_input_stream_;
  MockOutputStream mock_output_stream_;
};

TEST_F(AwdlBwuHandlerTest,
       CreateUpgradedEndpointChannel_InvalidCredentials_Fails) {
  ClientProxy client(&mock_event_logger_);
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  path_info.mutable_awdl_credentials();  // Empty credentials

  auto result =
      static_cast<BwuHandler*>(&handler_)->CreateUpgradedEndpointChannel(
          &client, "service_id", "endpoint_id", path_info);

  ASSERT_TRUE(result.has_error());
  EXPECT_EQ(result.error().operation_result_code().value(),
            OperationResultCode::CONNECTIVITY_AWDL_INVALID_CREDENTIAL);
}

TEST_F(AwdlBwuHandlerTest, CreateUpgradedEndpointChannel_Success) {
  ClientProxy client(&mock_event_logger_);
  client.AddCancellationFlag(std::string(kEndpointId));
  MockInputStream input_stream;
  MockOutputStream output_stream;
  auto awdl_socket = std::make_unique<MockAwdlSocket>();
  EXPECT_CALL(*awdl_socket, GetInputStream())
      .WillRepeatedly(ReturnRef(input_stream));
  EXPECT_CALL(*awdl_socket, GetOutputStream())
      .WillRepeatedly(ReturnRef(output_stream));

  EXPECT_CALL(*awdl_medium_mock, StartDiscovery(_, _))
      .WillOnce([](const std::string& service_type,
                   api::AwdlMedium::DiscoveredServiceCallback callback) {
        NsdServiceInfo service_info;
        service_info.SetServiceName(std::string(kServiceName));
        service_info.SetServiceType(service_type);
        if (callback.service_discovered_cb) {
          NsdServiceInfo service_info_copy = service_info;
          callback.service_discovered_cb(service_info_copy);
        }
        return true;
      });
  EXPECT_CALL(*awdl_medium_mock, StopDiscovery(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*awdl_medium_mock, ConnectToService(_, _, _))
      .WillOnce(Return(ByMove(std::move(awdl_socket))));

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  auto* credentials = path_info.mutable_awdl_credentials();
  credentials->set_service_name(kServiceName);
  credentials->set_service_type(kServiceType);
  credentials->set_password(kPassword);

  auto result =
      static_cast<BwuHandler*>(&handler_)->CreateUpgradedEndpointChannel(
          &client, std::string(kServiceId), std::string(kEndpointId),
          path_info);

  EXPECT_TRUE(result.has_value());
}

TEST_F(AwdlBwuHandlerTest,
       InitializeUpgradedMediumForEndpoint_StartAcceptingConnectionsFails) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ClientProxy client(&mock_event_logger_);
  client.AddCancellationFlag(std::string(kEndpointId));

  EXPECT_CALL(*awdl_medium_mock, ListenForService(_, 0))
      .WillOnce(Return(ByMove(nullptr)));

  ByteArray result = handler_.InitializeUpgradedMediumForEndpoint(
      &client, std::string(kServiceId), std::string(kEndpointId));

  EXPECT_TRUE(result.Empty());
  MediumEnvironment::Instance().Stop();
}

TEST_F(AwdlBwuHandlerTest, InitializeUpgradedMediumForEndpoint_Success) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  // The reason for putting ClientProxy inside a C++ { } block so it destructs
  // before the simulated clock is restored. Otherwise, the simulated clock
  // would stopped before ClientProxy went out of scope, causing its destructor
  // to log the session duration using the real system clock. If 1 or more
  // real-world milliseconds elapsed between the test start and test end, this
  // duration evaluated to something > 0.
  {
    ClientProxy client(&mock_event_logger_);
    client.GetAnalyticsRecorder().OnStartAdvertising(
        Strategy::kP2pPointToPoint,
        {location::nearby::proto::connections::Medium::BLUETOOTH},
        /*advertising_metadata_params=*/nullptr);
    client.GetAnalyticsRecorder().OnBandwidthUpgradeStarted(
        std::string(kEndpointId),
        location::nearby::proto::connections::Medium::BLUETOOTH,
        location::nearby::proto::connections::Medium::AWDL,
        location::nearby::proto::connections::ConnectionAttemptDirection::
            OUTGOING,
        /*connection_token=*/"");
    client.AddCancellationFlag(std::string(kEndpointId));

    auto awdl_server_socket = std::make_unique<MockAwdlServerSocket>();
    auto* awdl_server_socket_ptr = awdl_server_socket.get();
    EXPECT_CALL(*awdl_server_socket_ptr, GetPort())
        .WillRepeatedly(Return(8080));
    EXPECT_CALL(*awdl_server_socket_ptr, Accept())
        .WillOnce(Return(ByMove(nullptr)));
    EXPECT_CALL(*awdl_server_socket_ptr, Close())
        .WillRepeatedly(Return(Exception{Exception::kSuccess}));

    EXPECT_CALL(*awdl_medium_mock, ListenForService(_, 0))
        .WillOnce(Return(ByMove(std::move(awdl_server_socket))));

    std::string captured_service_name;
    std::string captured_service_type;
    EXPECT_CALL(*awdl_medium_mock, StartAdvertising(_))
        .WillOnce([&](const NsdServiceInfo& nsd_service_info) {
          captured_service_name = nsd_service_info.GetServiceName();
          captured_service_type = nsd_service_info.GetServiceType();
          return true;
        });

    ByteArray result = handler_.InitializeUpgradedMediumForEndpoint(
        &client, std::string(kServiceId), std::string(kEndpointId));

    EXPECT_FALSE(result.Empty());
    OfflineFrame expected_frame;
    expected_frame.set_version(OfflineFrame::V1);
    expected_frame.mutable_v1()->set_type(
        V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
    auto* bwu_frame =
        expected_frame.mutable_v1()->mutable_bandwidth_upgrade_negotiation();
    bwu_frame->set_event_type(
        BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
    auto* upgrade_path_info = bwu_frame->mutable_upgrade_path_info();
    upgrade_path_info->set_medium(
        BandwidthUpgradeNegotiationFrame::UpgradePathInfo::AWDL);
    upgrade_path_info->set_supports_client_introduction_ack(true);
    upgrade_path_info->set_supports_disabling_encryption(true);
    auto* awdl_credentials = upgrade_path_info->mutable_awdl_credentials();
    awdl_credentials->set_service_name(captured_service_name);
    awdl_credentials->set_service_type(captured_service_type);

    // The password is automatically generated and set in the handle start, we
    // can obtain it from the credential Since we mock StartAcceptingConnections
    // instead of using real awdl, GetPskInfo won't work perfectly. However
    // InitializeUpgradedMediumForEndpoint internally calls
    // parser::ForBwuAwdlPathAvailable which puts the generated password. We
    // will extract it from result directly to build expected frame.
    OfflineFrame result_frame;
    EXPECT_TRUE(result_frame.ParseFromString(std::string(result)));
    awdl_credentials->set_password(result_frame.v1()
                                       .bandwidth_upgrade_negotiation()
                                       .upgrade_path_info()
                                       .awdl_credentials()
                                       .password());

    EXPECT_THAT(result_frame, EqualsProto(expected_frame));

    constexpr absl::string_view kClientSessionLog = R"pb(
      event_type: CLIENT_SESSION
      client_session { duration_millis: 0 }
      version: "v1.5.0"
    )pb";
    constexpr absl::string_view kExpectedUpgradeLog = R"pb(
      event_type: CLIENT_SESSION
      client_session {
        duration_millis: 0
        strategy_session {
          duration_millis: 0
          strategy: P2P_POINT_TO_POINT
          role: ADVERTISER
          advertising_phase {
            duration_millis: 0
            medium: BLUETOOTH
            advertising_metadata {
              supports_extended_ble_advertisements: false
              connected_ap_frequency: 0
              supports_nfc_technology: false
            }
            stop_reason: FINISH_SESSION_STOP_ADVERTISING
          }
          upgrade_attempt {
            direction: OUTGOING
            duration_millis: 0
            from_medium: BLUETOOTH
            to_medium: AWDL
            upgrade_result: UNFINISHED_ERROR
            error_stage: UPGRADE_UNFINISHED
            connection_token: ""
            operation_result {
              result_category: CATEGORY_DEVICE_STATE_ERROR
              result_code: DEVICE_STATE_ERROR_UNFINISHED_UPGRADE_ATTEMPTS
            }
          }
        }
      }
      version: "v1.5.0"
    )pb";
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const ConnectionsLog&>(
                    HasEventType(EventType::STOP_STRATEGY_SESSION))))
        .Times(1);
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const ConnectionsLog&>(
                    HasEventType(EventType::STOP_CLIENT_SESSION))))
        .Times(3);
    EXPECT_CALL(mock_event_logger_,
                Log(Matcher<const ConnectionsLog&>(
                    HasEventType(EventType::START_CLIENT_SESSION))))
        .Times(3);
    EXPECT_CALL(
        mock_event_logger_,
        Log(Matcher<const ConnectionsLog&>(EqualsProto(kClientSessionLog))))
        .Times(2);
    EXPECT_CALL(
        mock_event_logger_,
        Log(Matcher<const ConnectionsLog&>(EqualsProto(kExpectedUpgradeLog))));
    // Flush pending logs.
    client.GetAnalyticsRecorder().LogSession();
    handler_.RevertInitiatorState();
  }
  MediumEnvironment::Instance().Stop();
}

TEST_F(AwdlBwuHandlerTest, OnIncomingAwdlConnection_Success) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ClientProxy client(&mock_event_logger_);
  client.AddCancellationFlag(std::string(kEndpointId));

  auto awdl_server_socket = std::make_unique<MockAwdlServerSocket>();
  auto* awdl_server_socket_ptr = awdl_server_socket.get();
  EXPECT_CALL(*awdl_server_socket_ptr, Close())
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*awdl_server_socket_ptr, Accept())
      .WillOnce([this]() {
        auto awdl_socket = std::make_unique<MockAwdlSocket>();
        EXPECT_CALL(*awdl_socket, GetInputStream())
            .WillRepeatedly(ReturnRef(mock_input_stream_));
        EXPECT_CALL(*awdl_socket, GetOutputStream())
            .WillRepeatedly(ReturnRef(mock_output_stream_));
        return awdl_socket;
      })
      .WillRepeatedly([]() {
        absl::SleepFor(absl::Seconds(5));
        return nullptr;
      });

  EXPECT_CALL(*awdl_medium_mock, ListenForService(_, 0))
      .WillOnce(Return(ByMove(std::move(awdl_server_socket))));
  EXPECT_CALL(*awdl_medium_mock, StartAdvertising(_)).WillOnce(Return(true));

  CountDownLatch latch(1);
  EXPECT_CALL(incoming_connection_callback_, Call(&client, _))
      .WillOnce([&latch](ClientProxy* client,
                         std::unique_ptr<BwuHandler::IncomingSocketConnection>
                             connection) { latch.CountDown(); });

  ByteArray result = handler_.InitializeUpgradedMediumForEndpoint(
      &client, std::string(kServiceId), std::string(kEndpointId));
  EXPECT_FALSE(result.Empty());

  auto await_result = latch.Await(absl::Seconds(5));
  EXPECT_TRUE(await_result.ok());

  handler_.RevertInitiatorState();
  MediumEnvironment::Instance().Stop();
}

TEST_F(AwdlBwuHandlerTest, AwdlIncomingSocket_ToStringAndClose) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ClientProxy client(&mock_event_logger_);
  client.AddCancellationFlag(std::string(kEndpointId));

  auto awdl_server_socket = std::make_unique<MockAwdlServerSocket>();
  auto* awdl_server_socket_ptr = awdl_server_socket.get();
  EXPECT_CALL(*awdl_server_socket_ptr, Close())
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*awdl_server_socket_ptr, Accept())
      .WillOnce([this]() {
        auto awdl_socket = std::make_unique<MockAwdlSocket>();
        EXPECT_CALL(*awdl_socket, GetInputStream())
            .WillRepeatedly(ReturnRef(mock_input_stream_));
        EXPECT_CALL(*awdl_socket, GetOutputStream())
            .WillRepeatedly(ReturnRef(mock_output_stream_));
        EXPECT_CALL(*awdl_socket, Close())
            .WillOnce(Return(Exception{Exception::kSuccess}));
        return awdl_socket;
      })
      .WillRepeatedly([]() {
        absl::SleepFor(absl::Seconds(5));
        return nullptr;
      });

  EXPECT_CALL(*awdl_medium_mock, ListenForService(_, 0))
      .WillOnce(Return(ByMove(std::move(awdl_server_socket))));
  EXPECT_CALL(*awdl_medium_mock, StartAdvertising(_)).WillOnce(Return(true));

  CountDownLatch latch(1);
  EXPECT_CALL(incoming_connection_callback_, Call(&client, _))
      .WillOnce([&latch](ClientProxy* client,
                         std::unique_ptr<BwuHandler::IncomingSocketConnection>
                             connection) {
        EXPECT_FALSE(connection->socket->ToString().empty());
        connection->socket->Close();
        latch.CountDown();
      });

  ByteArray result = handler_.InitializeUpgradedMediumForEndpoint(
      &client, std::string(kServiceId), std::string(kEndpointId));
  EXPECT_FALSE(result.Empty());

  auto await_result = latch.Await(absl::Seconds(5));
  EXPECT_TRUE(await_result.ok());

  handler_.RevertInitiatorState();
  MediumEnvironment::Instance().Stop();
}

TEST_F(AwdlBwuHandlerTest, HandleRevertInitiatorStateForService_Success) {
  MediumEnvironment::Instance().Start({.use_simulated_clock = true});
  ClientProxy client(&mock_event_logger_);
  client.AddCancellationFlag(std::string(kEndpointId));

  auto awdl_server_socket = std::make_unique<MockAwdlServerSocket>();
  auto* awdl_server_socket_ptr = awdl_server_socket.get();
  EXPECT_CALL(*awdl_server_socket_ptr, Close())
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*awdl_server_socket_ptr, Accept()).WillRepeatedly([]() {
    absl::SleepFor(absl::Seconds(10));
    return nullptr;
  });

  EXPECT_CALL(*awdl_medium_mock, ListenForService(_, 0))
      .WillOnce(Return(ByMove(std::move(awdl_server_socket))));
  EXPECT_CALL(*awdl_medium_mock, StartAdvertising(_)).WillOnce(Return(true));
  EXPECT_CALL(*awdl_medium_mock, StopAdvertising(_)).WillOnce(Return(true));

  handler_.InitializeUpgradedMediumForEndpoint(&client, std::string(kServiceId),
                                               std::string(kEndpointId));

  handler_.RevertInitiatorState();
  MediumEnvironment::Instance().Stop();
}

TEST_F(AwdlBwuHandlerTest, GetUpgradeMedium_ReturnsAwdl) {
  auto* bwu_handler = static_cast<BwuHandler*>(&handler_);
  EXPECT_EQ(bwu_handler->GetUpgradeMedium(),
            location::nearby::proto::connections::Medium::AWDL);
}

TEST_F(AwdlBwuHandlerTest, OnEndpointDisconnect_DoesNotCrash) {
  ClientProxy client(&mock_event_logger_);
  auto* bwu_handler = static_cast<BwuHandler*>(&handler_);
  // This method is a no-op, just verifying it doesn't crash.
  bwu_handler->OnEndpointDisconnect(&client, std::string(kEndpointId));
}

class AwdlEndpointChannelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(*awdl_medium_mock, IsNetworkConnected())
        .WillByDefault(Return(true));
    mock_socket_ = std::make_unique<StrictMock<MockAwdlSocket>>();
    EXPECT_CALL(*mock_socket_, GetInputStream())
        .WillRepeatedly(ReturnRef(mock_input_stream_));
    EXPECT_CALL(*mock_socket_, GetOutputStream())
        .WillRepeatedly(ReturnRef(mock_output_stream_));
  }

  std::unique_ptr<StrictMock<MockAwdlSocket>> mock_socket_;
  StrictMock<MockInputStream> mock_input_stream_;
  StrictMock<MockOutputStream> mock_output_stream_;
  Awdl awdl_medium_;
};

TEST_F(AwdlEndpointChannelTest, CloseImpl_StopsDiscoveryIfOutgoing) {
  EXPECT_CALL(*mock_socket_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(mock_input_stream_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(mock_output_stream_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*awdl_medium_mock, StartDiscovery(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*awdl_medium_mock, StopDiscovery(_)).WillOnce(Return(true));
  awdl_medium_.StartDiscovery(std::string(kServiceId), {});

  AwdlEndpointChannel channel(
      std::string(kServiceId), std::string(kChannelName),
      AwdlSocket(std::move(mock_socket_)), &awdl_medium_,
      /*is_outgoing=*/true);

  channel.Close(location::nearby::proto::connections::DisconnectionReason::
                    UNKNOWN_DISCONNECTION_REASON);
}

TEST_F(AwdlEndpointChannelTest, CloseImpl_DoesNotStopDiscoveryIfIncoming) {
  EXPECT_CALL(*mock_socket_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(mock_input_stream_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(mock_output_stream_, Close())
      .WillOnce(Return(Exception{Exception::kSuccess}));

  AwdlEndpointChannel channel(
      std::string(kServiceId), std::string(kChannelName),
      AwdlSocket(std::move(mock_socket_)), &awdl_medium_,
      /*is_outgoing=*/false);

  channel.Close(location::nearby::proto::connections::DisconnectionReason::
                    UNKNOWN_DISCONNECTION_REASON);
}

TEST_F(AwdlEndpointChannelTest, EnableMultiplexSocket_CallsSocket) {
  AwdlEndpointChannel channel(
      std::string(kServiceId), std::string(kChannelName),
      AwdlSocket(std::move(mock_socket_)), &awdl_medium_,
      /*is_outgoing=*/true);

  EXPECT_TRUE(channel.EnableMultiplexSocket());
}

}  // namespace
}  // namespace connections
namespace api {

std::unique_ptr<AwdlMedium> ImplementationPlatform::CreateAwdlMedium() {
  auto medium = std::make_unique<MockAwdlMedium>();
  awdl_medium_mock = medium.get();
  return medium;
}

}  // namespace api
}  // namespace nearby
