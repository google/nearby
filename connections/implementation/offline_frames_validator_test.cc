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

#include "connections/implementation/offline_frames_validator.h"

#include <array>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace parser {
namespace {

using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::connections::PayloadTransferFrame;

constexpr absl::string_view kEndpointId{"ABC"};
constexpr absl::string_view kEndpointName{"XYZ"};
constexpr int kNonce = 1234;
constexpr bool kSupports5ghz = true;
constexpr absl::string_view kBssid{"FF:FF:FF:FF:FF:FF"};
constexpr int kApFrequency = 2412;
constexpr absl::string_view kIp4Bytes = {"8xqT"};
constexpr int kStatusAccepted = 0;
constexpr absl::string_view kSsid = "ssid";
constexpr absl::string_view kPassword = "password";
constexpr absl::string_view kWifiHotspotGateway = "0.0.0.0";
constexpr absl::string_view kWifiDirectSsid = "DIRECT-A0-0123456789AB";
constexpr absl::string_view kWifiDirectPassword = "WIFIDIRECT123456";
constexpr absl::string_view kGateway = "192.168.1.1";
constexpr int kWifiDirectFrequency = 2412;
constexpr int kPort = 1000;
constexpr bool kSupportsDisablingEncryption = true;
constexpr std::array<Medium, 9> kMediums = {
    Medium::MDNS, Medium::BLUETOOTH,   Medium::WIFI_HOTSPOT,
    Medium::BLE,  Medium::WIFI_LAN,    Medium::WIFI_AWARE,
    Medium::NFC,  Medium::WIFI_DIRECT, Medium::WEB_RTC,
};
constexpr int kKeepAliveIntervalMillis = 1000;
constexpr int kKeepAliveTimeoutMillis = 5000;

class OfflineFramesConnectionRequestTest : public testing::Test {
 protected:
  ConnectionInfo connection_info_{std::string(kEndpointId),
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
};

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithValidConnectionRequestFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullConnectionRequestFrame) {
  OfflineFrame offline_frame;

  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_request();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullEndpointIdInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.local_endpoint_id = "";
  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullEndpointInfoInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.local_endpoint_info = ByteArray{""};
  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithNullBssidInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.bssid = "";
  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithNullMediumsInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.supported_mediums = {};
  ByteArray bytes = ForConnectionRequest(connection_info_);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithValidConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  ByteArray bytes = ForConnectionResponse(kStatusAccepted, os_info);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  ByteArray bytes = ForConnectionResponse(kStatusAccepted, os_info);
  offline_frame.ParseFromString(std::string(bytes));
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_response();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithUnexpectedStatusInConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  ByteArray bytes = ForConnectionResponse(-1, os_info);
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
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkTypeFileWithEmptyFilePathAndParent) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  header.set_file_name(std::string());
  header.set_parent_folder(std::string());
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsOkTypeFileWithLegalFilePath) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  header.set_file_name(
      std::string("earth_85MB_test (1) (3) (4) (8) (1) (2) (2) (1).jpg"));
  header.set_parent_folder(std::string());
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsFailedTypeFileWithIllegalFilePath) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  header.set_file_name(
      std::string("earth_85MB_test (1): (3) (4) (8) (1) (2) (2) (1).jpg"));
  header.set_parent_folder(std::string());
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.value == Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest, ValidatesAsOkTypeFileWithLegalParentFolder) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  header.set_file_name("");
  header.set_parent_folder(std::string(
      std::string("earth_85MB_test (1) (3) (4) (8) (1) (2) (2) (1).jpg")));
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailedTypeFileWithIllegalParentFolder) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  // Sending files larger than 2gb was previously broken (see cl/372382338).
  // This tests a file larger than int max.
  header.set_total_size(3e10);
  header.set_file_name("");
  header.set_parent_folder(std::string(
      std::string("earth_85MB_test (1): (3) (4) (8) (1) (2) (2) (1).jpg")));
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  ByteArray bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.value == Exception::kIllegalCharacters);
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
      kWifiDirectFrequency, kSupportsDisablingEncryption,
      std::string(kGateway));
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
      kSupportsDisablingEncryption, std::string(kGateway));
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  // But -1 itself is not invalid
  bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort, -1,
      kSupportsDisablingEncryption, std::string(kGateway));
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
      kWifiDirectFrequency, kSupportsDisablingEncryption,
      std::string(kGateway));
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  std::string wifi_direct_ssid_wrong_length =
      std::string{kWifiDirectSsid} + "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
  bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid_wrong_length, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption,
      std::string(kGateway));
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
      kWifiDirectFrequency, kSupportsDisablingEncryption,
      std::string(kGateway));
  offline_frame_1.ParseFromString(std::string(bytes));

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  ASSERT_FALSE(ret_value.Ok());

  std::string long_wifi_direct_password =
      std::string{kWifiDirectSsid} +
      "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
  bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), long_wifi_direct_password, kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption,
      std::string(kGateway));
  offline_frame_2.ParseFromString(std::string(bytes));

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  ASSERT_FALSE(ret_value.Ok());
}

}  // namespace
}  // namespace parser
}  // namespace connections
}  // namespace nearby
