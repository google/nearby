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

#include "core_v2/internal/offline_frames.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {
namespace {

using Medium = proto::connections::Medium;
using ::testing::EqualsProto;

constexpr absl::string_view kEndpointId{"ABC"};
constexpr absl::string_view kEndpointName{"XYZ"};
constexpr int kNonce = 1234;
constexpr std::array<Medium, 9> kMediums = {
    Medium::MDNS, Medium::BLUETOOTH,   Medium::WIFI_HOTSPOT,
    Medium::BLE,  Medium::WIFI_LAN,    Medium::WIFI_AWARE,
    Medium::NFC,  Medium::WIFI_DIRECT, Medium::WEB_RTC,
};

TEST(OfflineFramesTest, CanParseMessageFromBytes) {
  OfflineFrame tx_message;

  {
    tx_message.set_version(OfflineFrame::V1);
    auto* v1_frame = tx_message.mutable_v1();
    auto* sub_frame = v1_frame->mutable_connection_request();

    v1_frame->set_type(V1Frame::CONNECTION_REQUEST);
    sub_frame->set_endpoint_id(kEndpointId);
    sub_frame->set_endpoint_name(kEndpointName);
    sub_frame->set_endpoint_info(kEndpointName);
    sub_frame->set_nonce(kNonce);
    for (auto& medium : kMediums) {
      sub_frame->add_mediums(MediumToConnectionRequestMedium(medium));
    }
  }
  auto serialized_bytes = ByteArray(tx_message.SerializeAsString());
  auto ret_value = FromBytes(serialized_bytes);
  ASSERT_TRUE(ret_value.ok());
  const auto& rx_message = ret_value.result();
  EXPECT_THAT(rx_message, EqualsProto(tx_message));
  EXPECT_EQ(GetFrameType(rx_message), V1Frame::CONNECTION_REQUEST);
  EXPECT_EQ(
      ConnectionRequestMediumsToMediums(rx_message.v1().connection_request()),
      std::vector(kMediums.begin(), kMediums.end()));
}

TEST(OfflineFramesTest, CanGenerateConnectionRequest) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: CONNECTION_REQUEST
      connection_request: <
        endpoint_id: "ABC"
        endpoint_name: "XYZ"
        endpoint_info: "XYZ"
        nonce: 1234
        mediums: MDNS
        mediums: BLUETOOTH
        mediums: WIFI_HOTSPOT
        mediums: BLE
        mediums: WIFI_LAN
        mediums: WIFI_AWARE
        mediums: NFC
        mediums: WIFI_DIRECT
        mediums: WEB_RTC
      >
    >)pb";
  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), ByteArray{std::string(kEndpointName)}, kNonce,
      std::vector(kMediums.begin(), kMediums.end()));
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateConnectionResponse) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: CONNECTION_RESPONSE
      connection_response: < status: 1 >
    >)pb";
  ByteArray bytes = ForConnectionResponse(1);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateControlPayloadTransfer) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::ControlMessage control;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  control.set_event(PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  control.set_offset(150);

  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: PAYLOAD_TRANSFER
      payload_transfer: <
        packet_type: CONTROL,
        payload_header: < type: BYTES id: 12345 total_size: 1024 >
        control_message: < event: PAYLOAD_CANCELED offset: 150 >
      >
    >)pb";
  ByteArray bytes = ForControlPayloadTransfer(header, control);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateDataPayloadTransfer) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: PAYLOAD_TRANSFER
      payload_transfer: <
        packet_type: DATA,
        payload_header: < type: BYTES id: 12345 total_size: 1024 >
        payload_chunk: < flags: 1 offset: 150 body: "payload data" >
      >
    >)pb";
  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuWifiHotspotPathAvailable) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: UPGRADE_PATH_AVAILABLE
        upgrade_path_info: <
          medium: WIFI_HOTSPOT
          wifi_hotspot_credentials: <
            ssid: "ssid"
            password: "password"
            port: 1234
          >
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiHotspotPathAvailable("ssid", "password", 1234);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuWifiLanPathAvailable) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: UPGRADE_PATH_AVAILABLE
        upgrade_path_info: <
          medium: WIFI_LAN
          wifi_lan_socket: < ip_address: "\x01\x02\x03\x04" wifi_port: 1234 >
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiLanPathAvailable("\x01\x02\x03\x04", 1234);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuBluetoothPathAvailable) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: UPGRADE_PATH_AVAILABLE
        upgrade_path_info: <
          medium: BLUETOOTH
          bluetooth_credentials: <
            service_name: "service"
            mac_address: "\x11\x22\x33\x44\x55\x66"
          >
        >
      >
    >)pb";
  ByteArray bytes =
      ForBwuBluetoothPathAvailable("service", "\x11\x22\x33\x44\x55\x66");
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuLastWrite) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: < event_type: LAST_WRITE_TO_PRIOR_CHANNEL >
    >)pb";
  ByteArray bytes = ForBwuLastWrite();
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuSafeToClose) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: < event_type: SAFE_TO_CLOSE_PRIOR_CHANNEL >
    >)pb";
  ByteArray bytes = ForBwuSafeToClose();
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuIntroduction) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: CLIENT_INTRODUCTION
        client_introduction: < endpoint_id: "ABC" >
      >
    >)pb";
  ByteArray bytes = ForBwuIntroduction(std::string(kEndpointId));
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateKeepAlive) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: KEEP_ALIVE
      keep_alive: <>
    >)pb";
  ByteArray bytes = ForKeepAlive();
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

}  // namespace
}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location
