// Copyright 2025 Google LLC
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

#include "connections/implementation/wifi_lan_bwu_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/mediums.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/mock_input_stream.h"
#include "internal/platform/mock_output_stream.h"
#include "internal/platform/mock_wifi_lan_medium.h"
#include "internal/platform/mock_wifi_lan_server_socket.h"
#include "internal/platform/mock_wifi_lan_socket.h"
#include "internal/platform/service_address.h"

namespace nearby {

MockWifiLanMedium* wifi_lan_medium = nullptr;

namespace connections {
namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::OperationResultCode;
using ::testing::_;
using ::testing::ByMove;
using ::protobuf_matchers::EqualsProto;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr absl::string_view kIpv4Address{"\xc0\xa8\x00\x01", 4};
constexpr absl::string_view kIpv6Address{
    "\x2a\x00\x79\xe0\x2e\x87\x00\x06\xb7\x28\x67\x45\x7a\xdd\x01\x53", 16};
constexpr absl::string_view kServiceId{"service_id"};
constexpr absl::string_view kEndpointId{"endpoint_id"};

class WifiLanBwuHandlerTest : public ::testing::Test {
 protected:
  WifiLanBwuHandlerTest()
      : handler_(mediums_, incoming_connection_callback_.AsStdFunction()) {}

  Mediums mediums_;
  MockFunction<void(ClientProxy*,
                    std::unique_ptr<BwuHandler::IncomingSocketConnection>)>
      incoming_connection_callback_;
  WifiLanBwuHandler handler_;
};

TEST_F(WifiLanBwuHandlerTest,
       CreateUpgradedEndpointChannel_EmptyPathInfo_Fails) {
  ClientProxy client;
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  // Create an empty wifi_lan_socket.
  path_info.mutable_wifi_lan_socket();

  auto result = handler_.CreateUpgradedEndpointChannel(
      &client, "service_id", "endpoint_id", std::move(path_info));

  ASSERT_TRUE(result.has_error());
  EXPECT_EQ(result.error().operation_result_code().value(),
            OperationResultCode::CONNECTIVITY_WIFI_LAN_IP_ADDRESS_ERROR);
};

TEST_F(WifiLanBwuHandlerTest, CreateUpgradedEndpointChannel_IpAddress_Success) {
  ClientProxy client;
  client.AddCancellationFlag(std::string(kEndpointId));
  MockInputStream input_stream;
  MockOutputStream output_stream;
  auto wifi_lan_socket = std::make_unique<MockWifiLanSocket>();
  EXPECT_CALL(*wifi_lan_socket, GetInputStream())
      .WillRepeatedly(ReturnRef(input_stream));
  EXPECT_CALL(*wifi_lan_socket, GetOutputStream())
      .WillRepeatedly(ReturnRef(output_stream));
  EXPECT_CALL(*wifi_lan_medium, IsNetworkConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*wifi_lan_medium,
              ConnectToService(std::string(kIpv4Address), 8080, _))
      .WillOnce(Return(ByMove(std::move(wifi_lan_socket))));

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  path_info.mutable_wifi_lan_socket()->set_ip_address(kIpv4Address);
  path_info.mutable_wifi_lan_socket()->set_wifi_port(8080);

  auto result = handler_.CreateUpgradedEndpointChannel(
      &client, std::string(kServiceId), std::string(kEndpointId),
      std::move(path_info));

  EXPECT_TRUE(result.has_value());
};

TEST_F(WifiLanBwuHandlerTest,
       CreateUpgradedEndpointChannel_AddressCandidates_FirstCandidate_Success) {
  ClientProxy client;
  client.AddCancellationFlag(std::string(kEndpointId));
  MockInputStream input_stream;
  MockOutputStream output_stream;
  auto wifi_lan_socket = std::make_unique<MockWifiLanSocket>();
  EXPECT_CALL(*wifi_lan_socket, GetInputStream())
      .WillRepeatedly(ReturnRef(input_stream));
  EXPECT_CALL(*wifi_lan_socket, GetOutputStream())
      .WillRepeatedly(ReturnRef(output_stream));
  EXPECT_CALL(*wifi_lan_medium, IsNetworkConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*wifi_lan_medium,
              ConnectToService(std::string(kIpv6Address), 8080, _))
      .WillOnce(Return(ByMove(std::move(wifi_lan_socket))));

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  auto* address_candidate =
      path_info.mutable_wifi_lan_socket()->add_address_candidates();
  address_candidate->set_ip_address(kIpv6Address);
  address_candidate->set_port(8080);
  address_candidate =
      path_info.mutable_wifi_lan_socket()->add_address_candidates();
  address_candidate->set_ip_address(kIpv4Address);
  address_candidate->set_port(8080);

  auto result = handler_.CreateUpgradedEndpointChannel(
      &client, std::string(kServiceId), std::string(kEndpointId),
      std::move(path_info));

  EXPECT_TRUE(result.has_value());
};

TEST_F(WifiLanBwuHandlerTest,
       CreateUpgradedEndpointChannel_AddressCandidates_FirstCandidate_Fails) {
  ClientProxy client;
  client.AddCancellationFlag(std::string(kEndpointId));
  MockInputStream input_stream;
  MockOutputStream output_stream;
  auto wifi_lan_socket = std::make_unique<MockWifiLanSocket>();
  EXPECT_CALL(*wifi_lan_socket, GetInputStream())
      .WillRepeatedly(ReturnRef(input_stream));
  EXPECT_CALL(*wifi_lan_socket, GetOutputStream())
      .WillRepeatedly(ReturnRef(output_stream));
  EXPECT_CALL(*wifi_lan_medium, IsNetworkConnected())
      .WillRepeatedly(Return(true));
  InSequence seq;
  EXPECT_CALL(*wifi_lan_medium,
              ConnectToService(std::string(kIpv6Address), 8080, _))
      .WillOnce(Return(ByMove(nullptr)));
  EXPECT_CALL(*wifi_lan_medium,
              ConnectToService(std::string(kIpv4Address), 8080, _))
      .WillOnce(Return(ByMove(std::move(wifi_lan_socket))));

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo path_info;
  auto* address_candidate =
      path_info.mutable_wifi_lan_socket()->add_address_candidates();
  address_candidate->set_ip_address(kIpv6Address);
  address_candidate->set_port(8080);
  address_candidate =
      path_info.mutable_wifi_lan_socket()->add_address_candidates();
  address_candidate->set_ip_address(kIpv4Address);
  address_candidate->set_port(8080);

  auto result = handler_.CreateUpgradedEndpointChannel(
      &client, std::string(kServiceId), std::string(kEndpointId),
      std::move(path_info));

  EXPECT_TRUE(result.has_value());
};

TEST_F(WifiLanBwuHandlerTest, InitializeUpgradedMediumForEndpoint_Success) {
  ClientProxy client;
  client.AddCancellationFlag(std::string(kEndpointId));
  auto wifi_lan_server_socket = std::make_unique<MockWifiLanServerSocket>();
  EXPECT_CALL(*wifi_lan_server_socket, GetPort()).WillRepeatedly(Return(8080));
  EXPECT_CALL(*wifi_lan_medium, IsNetworkConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*wifi_lan_medium, ListenForService(_))
      .WillOnce(Return(ByMove(std::move(wifi_lan_server_socket))));
  EXPECT_CALL(*wifi_lan_medium, GetUpgradeAddressCandidates(_))
      .WillOnce(Return(std::vector<ServiceAddress>{
          {std::vector<char>{kIpv6Address.begin(), kIpv6Address.end()}, 8080},
          {std::vector<char>{kIpv4Address.begin(), kIpv4Address.end()},
           8888}}));
  OfflineFrame expected_frame;
  expected_frame.set_version(OfflineFrame::V1);
  expected_frame.mutable_v1()->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* bwu_frame =
      expected_frame.mutable_v1()->mutable_bandwidth_upgrade_negotiation();
  bwu_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = bwu_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_LAN);
  auto* wifi_lan_socket = expected_frame.mutable_v1()
                              ->mutable_bandwidth_upgrade_negotiation()
                              ->mutable_upgrade_path_info()
                              ->mutable_wifi_lan_socket();
  wifi_lan_socket->set_ip_address(kIpv4Address);
  wifi_lan_socket->set_wifi_port(8888);
  auto* address_candidate = wifi_lan_socket->add_address_candidates();
  address_candidate->set_ip_address(kIpv6Address);
  address_candidate->set_port(8080);
  address_candidate = wifi_lan_socket->add_address_candidates();
  address_candidate->set_ip_address(kIpv4Address);
  address_candidate->set_port(8888);
  upgrade_path_info->set_supports_client_introduction_ack(true);

  ByteArray result = handler_.InitializeUpgradedMediumForEndpoint(
      &client, std::string(kServiceId), std::string(kEndpointId));

  EXPECT_FALSE(result.Empty());
  OfflineFrame result_frame;
  EXPECT_TRUE(result_frame.ParseFromString(std::string(result)));
  EXPECT_THAT(result_frame, EqualsProto(expected_frame));
}

}  // namespace
}  // namespace connections
namespace api {

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  auto medium = std::make_unique<MockWifiLanMedium>();
  wifi_lan_medium = medium.get();
  return medium;
}

}  // namespace api
}  // namespace nearby
