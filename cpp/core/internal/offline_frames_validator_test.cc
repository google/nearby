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

#include "core/internal/offline_frames_validator.h"

#include "core/internal/offline_frames.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/byte_array.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {
namespace {

constexpr absl::string_view kEndpointId{"ABC"};
constexpr absl::string_view kEndpointName{"XYZ"};
constexpr int kNonce = 1234;
constexpr bool kSupports5ghz = true;
constexpr absl::string_view kBssid{"FF:FF:FF:FF:FF:FF"};
constexpr int kStatusAccepted = 0;
constexpr absl::string_view kSsid = "ssid";
constexpr absl::string_view kPassword = "password";
constexpr absl::string_view kWifiHotspotGateway = "0.0.0.0";
constexpr absl::string_view kWifiDirectSsid = "DIRECT-A0-0123456789AB";
constexpr absl::string_view kWifiDirectPassword = "WIFIDIRECT123456";
constexpr int kWifiDirectFrequency = 1000;
constexpr int kPort = 1000;
constexpr bool kSupportsDisablingEncryption = true;
constexpr std::array<Medium, 9> kMediums = {
    Medium::MDNS, Medium::BLUETOOTH,   Medium::WIFI_HOTSPOT,
    Medium::BLE,  Medium::WIFI_LAN,    Medium::WIFI_AWARE,
    Medium::NFC,  Medium::WIFI_DIRECT, Medium::WEB_RTC,
};

TEST(OfflineFramesValidatorTest, ValidatesAsOkWithValidConnectionRequestFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), ByteArray{std::string(kEndpointName)}, kNonce,
      kSupports5ghz, std::string(kBssid),
      std::vector(kMediums.begin(), kMediums.end()));
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullConnectionRequestFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), ByteArray{std::string(kEndpointName)}, kNonce,
      kSupports5ghz, std::string(kBssid),
      std::vector(kMediums.begin(), kMediums.end()));
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_request();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullEndpointIdInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  std::string empty_enpoint_id;
  ByteArray bytes = ForConnectionRequest(
      empty_enpoint_id, ByteArray{std::string(kEndpointName)}, kNonce,
      kSupports5ghz, std::string(kBssid),
      std::vector(kMediums.begin(), kMediums.end()));
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullEndpointInfoInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  ByteArray empty_endpoint_info;
  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), empty_endpoint_info, kNonce, kSupports5ghz,
      std::string(kBssid), std::vector(kMediums.begin(), kMediums.end()));
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithNullBssidInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  std::string empty_bssid;
  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), ByteArray{std::string(kEndpointName)}, kNonce,
      kSupports5ghz, empty_bssid,
      std::vector(kMediums.begin(), kMediums.end()));
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithNullMediumsInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  std::vector<Medium> empty_mediums;
  ByteArray bytes = ForConnectionRequest(
      std::string(kEndpointId), ByteArray{std::string(kEndpointName)}, kNonce,
      kSupports5ghz, std::string(kBssid), empty_mediums);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithValidConnectionResponseFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionResponse(kStatusAccepted);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullConnectionResponseFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionResponse(kStatusAccepted);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_response();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithUnexpectedStatusInConnectionResponseFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionResponse(-1);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  // To maintain forward compatibility, we allow unexpected status codes.
  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsOkWithValidPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsFailWithNullPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_payload_transfer();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullPayloadHeaderInPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_payload_header();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidSizeInPayloadHeader) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(-5);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullPayloadChunkInPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_payload_chunk();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidOffsetInPayloadChunk) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(-1);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidLargeOffsetInPayloadChunk) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(4999);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidFlagsInPayloadChunk) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();
  auto* payload_chunk = payload_transfer->mutable_payload_chunk();

  payload_chunk->clear_flags();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullControlMessageInPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::ControlMessage control;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  control.set_event(PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  control.set_offset(150);

  OfflineFrame offline_frame;

  ByteArray bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(std::string(bytes));

  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_control_message();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidNegativeOffsetInControlMessage) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::ControlMessage control;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  control.set_event(PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  control.set_offset(-1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidLargeOffsetInControlMessage) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::ControlMessage control;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  control.set_event(PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);
  control.set_offset(4999);

  OfflineFrame offline_frame;

  ByteArray bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithValidBandwidthUpgradeNegotiationFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForBwuWifiHotspotPathAvailable(
      std::string(kSsid), std::string(kPassword), kPort,
      std::string(kWifiHotspotGateway), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullBandwidthUpgradeNegotiationFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForBwuWifiHotspotPathAvailable(
      std::string(kSsid), std::string(kPassword), kPort,
      std::string(kWifiHotspotGateway), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_bandwidth_upgrade_negotiation();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsOkBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesValidFrequencyInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  // Anything less than -1 is invalid
  ByteArray bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort, -2,
      kSupportsDisablingEncryption);
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  // But -1 itself is not invalid
  bytes = ForBwuWifiDirectPathAvailable(std::string(kWifiDirectSsid),
                                        std::string(kWifiDirectPassword), kPort,
                                        -1, kSupportsDisablingEncryption);
  offline_frame_2.ParseFromString(std::string(bytes));

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidSsidInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  std::string wifi_direct_ssid{"DIRECT-A*-0123456789AB"};
  ByteArray bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption);
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  std::string wifi_direct_ssid_wrong_length =
      std::string{kWifiDirectSsid} + "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
  bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid_wrong_length, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption);
  offline_frame_2.ParseFromString(std::string(bytes));

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidPasswordInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  std::string short_wifi_direct_password{"Test"};
  ByteArray bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), short_wifi_direct_password, kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption);
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  std::string long_wifi_direct_password =
      std::string{kWifiDirectSsid} +
      "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
  bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), long_wifi_direct_password, kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption);
  offline_frame_2.ParseFromString(std::string(bytes));

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  ASSERT_FALSE(ret_value.Ok());
}

}  // namespace
}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location
