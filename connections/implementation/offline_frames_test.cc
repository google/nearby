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

#include "connections/implementation/offline_frames.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace parser {
namespace {

using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;
using Medium = ::location::nearby::proto::connections::Medium;
using ::protobuf_matchers::EqualsProto;

constexpr absl::string_view kEndpointId{"ABC"};
constexpr absl::string_view kEndpointName{"XYZ"};
constexpr int kNonce = 1234;
constexpr bool kSupports5ghz = true;
constexpr absl::string_view kBssid{"FF:FF:FF:FF:FF:FF"};
constexpr int kApFrequency = 2412;
constexpr absl::string_view kIp4Bytes = {"8xqT"};
constexpr std::array<Medium, 9> kMediums = {
    Medium::MDNS, Medium::BLUETOOTH,   Medium::WIFI_HOTSPOT,
    Medium::BLE,  Medium::WIFI_LAN,    Medium::WIFI_AWARE,
    Medium::NFC,  Medium::WIFI_DIRECT, Medium::WEB_RTC,
};
constexpr int kKeepAliveIntervalMillis = 1000;
constexpr int kKeepAliveTimeoutMillis = 5000;

TEST(OfflineFramesTest, CanParseMessageFromBytes) {
  OfflineFrame tx_message;

  {
    tx_message.set_version(OfflineFrame::V1);
    auto* v1_frame = tx_message.mutable_v1();
    auto* sub_frame = v1_frame->mutable_connection_request();

    v1_frame->set_type(V1Frame::CONNECTION_REQUEST);
    // OSS matchers don't like implicitly comparing string_views to strings.
    sub_frame->set_endpoint_id(std::string(kEndpointId));
    sub_frame->set_endpoint_name(std::string(kEndpointName));
    sub_frame->set_endpoint_info(std::string(kEndpointName));
    sub_frame->set_nonce(kNonce);
    sub_frame->set_keep_alive_interval_millis(kKeepAliveIntervalMillis);
    sub_frame->set_keep_alive_timeout_millis(kKeepAliveTimeoutMillis);
    auto* medium_metadata = sub_frame->mutable_medium_metadata();

    medium_metadata->set_supports_5_ghz(kSupports5ghz);
    medium_metadata->set_bssid(kBssid);

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
        medium_metadata: <
          supports_5_ghz: true
          bssid: "FF:FF:FF:FF:FF:FF"
          ip_address: "8xqT"
          ap_frequency: 2412
        >
        mediums: MDNS
        mediums: BLUETOOTH
        mediums: WIFI_HOTSPOT
        mediums: BLE
        mediums: WIFI_LAN
        mediums: WIFI_AWARE
        mediums: NFC
        mediums: WIFI_DIRECT
        mediums: WEB_RTC
        keep_alive_interval_millis: 1000
        keep_alive_timeout_millis: 5000
      >
    >)pb";

  ConnectionInfo connection_info{std::string(kEndpointId),
                                 ByteArray{std::string(kEndpointName)},
                                 kNonce,
                                 kSupports5ghz,
                                 std::string(kBssid),
                                 kApFrequency,
                                 std::string(kIp4Bytes),
                                 std::vector<Medium, std::allocator<Medium>>(
                                     kMediums.begin(), kMediums.end()),
                                 kKeepAliveIntervalMillis,
                                 kKeepAliveTimeoutMillis};
  ByteArray bytes = ForConnectionRequest(connection_info);
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
      connection_response: <
        status: 1
        response: REJECT
        os_info { type: LINUX }
      >
    >)pb";

  OsInfo os_info;
  os_info.set_type(OsInfo::LINUX);
  ByteArray bytes = ForConnectionResponse(1, os_info);
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
            gateway: "0.0.0.0"
          >
          supports_disabling_encryption: false
          supports_client_introduction_ack: true
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiHotspotPathAvailable("ssid", "password", 1234,
                                                   "0.0.0.0", false);
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
          supports_client_introduction_ack: true
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiLanPathAvailable("\x01\x02\x03\x04", 1234);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuWifiAwarePathAvailable) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: UPGRADE_PATH_AVAILABLE
        upgrade_path_info: <
          medium: WIFI_AWARE
          wifi_aware_credentials: <
            service_id: "service_id"
            service_info: "service_info"
            password: "password"
          >
          supports_disabling_encryption: false
          supports_client_introduction_ack: true
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiAwarePathAvailable("service_id", "service_info",
                                                 "password", false);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  OfflineFrame message = FromBytes(bytes).result();
  EXPECT_THAT(message, EqualsProto(kExpected));
}

TEST(OfflineFramesTest, CanGenerateBwuWifiDirectPathAvailable) {
  constexpr char kExpected[] =
      R"pb(
    version: V1
    v1: <
      type: BANDWIDTH_UPGRADE_NEGOTIATION
      bandwidth_upgrade_negotiation: <
        event_type: UPGRADE_PATH_AVAILABLE
        upgrade_path_info: <
          medium: WIFI_DIRECT
          wifi_direct_credentials: <
            ssid: "DIRECT-A0-0123456789AB"
            password: "password"
            port: 1000
            frequency: 2412
            gateway: "192.168.1.1"
          >
          supports_disabling_encryption: false
          supports_client_introduction_ack: true
        >
      >
    >)pb";
  ByteArray bytes = ForBwuWifiDirectPathAvailable(
      "DIRECT-A0-0123456789AB", "password", 1000, 2412, false, "192.168.1.1");
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
          supports_client_introduction_ack: true
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
        client_introduction: < 
          endpoint_id: "ABC"
          supports_disabling_encryption: false
        >
      >
    >)pb";
  ByteArray bytes = ForBwuIntroduction(
      std::string(kEndpointId), false /* supports_disabling_encryption */);
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
