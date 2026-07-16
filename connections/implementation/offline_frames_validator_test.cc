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
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/connection_options.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/medium_selector.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/service_address.h"

namespace nearby::connections::parser {
namespace {

using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;

constexpr absl::string_view kEndpointId{"ABC"};
constexpr absl::string_view kEndpointName{"XYZ"};
constexpr int kNonce = 1234;
constexpr bool kSupports5ghz = true;
constexpr absl::string_view kBssid{"FF:FF:FF:FF:FF:FF"};
constexpr int kApFrequency = 2412;
constexpr int kStatusAccepted = 0;
constexpr absl::string_view kSsid = "ssid";
constexpr absl::string_view kPassword = "password";
constexpr absl::string_view kWifiHotspotGateway = "0.0.0.0";
constexpr absl::string_view kWifiDirectSsid = "DIRECT-A0-0123456789AB";
constexpr absl::string_view kWifiDirectPassword = "WIFIDIRECT123456";
constexpr absl::string_view kWifiDirectDeviceName = "NC-WifiDirectTest";
constexpr absl::string_view kWifiDirectPin = "b592f7d3";
constexpr absl::string_view kGateway = "192.168.1.1";
constexpr int kWifiDirectFrequency = 2412;
constexpr int kPort = 1000;
constexpr int kHotspotFrequency = 2412;
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
  ConnectionInfo connection_info_{
      std::string(kEndpointId),
      ByteArray{std::string(kEndpointName)},
      kNonce,
      kSupports5ghz,
      std::string(kBssid),
      kApFrequency,
      std::vector<Medium>(kMediums.begin(), kMediums.end()),
      kKeepAliveIntervalMillis,
      kKeepAliveTimeoutMillis};
};

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithValidConnectionRequestFrame) {
  OfflineFrame offline_frame;

  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullConnectionRequestFrame) {
  OfflineFrame offline_frame;

  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_request();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullEndpointIdInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.local_endpoint_id = "";
  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithEmptyEndpointIdInConnectionRequestFrame) {
  connection_info_.local_endpoint_id = "";
  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  OfflineFrame frame;
  frame.ParseFromString(bytes);
  frame.mutable_v1()->mutable_connection_request()->set_endpoint_id("");
  ASSERT_TRUE(frame.v1().connection_request().has_endpoint_id());

  OfflineFrame offline_frame;
  offline_frame.ParseFromString(frame.SerializeAsString());

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsFailWithNullEndpointInfoInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.local_endpoint_info = ByteArray{""};
  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithNullBssidInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.bssid = "";
  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST_F(OfflineFramesConnectionRequestTest,
       ValidatesAsOkWithNullMediumsInConnectionRequestFrame) {
  OfflineFrame offline_frame;

  connection_info_.supported_mediums = {};
  std::string bytes = ForConnectionRequestConnections({}, connection_info_);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsOkWithValidConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  std::string bytes =
      ForConnectionResponse(kStatusAccepted, os_info, "device_name");
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  std::string bytes =
      ForConnectionResponse(kStatusAccepted, os_info, "device_name");
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_connection_response();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  ASSERT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithUnexpectedStatusInConnectionResponseFrame) {
  OfflineFrame offline_frame;

  OsInfo os_info;
  std::string bytes = ForConnectionResponse(-1, os_info, "device_name");
  offline_frame.ParseFromString(bytes);

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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_TRUE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest, ValidatesAsFailedTypeFileWithNonUtf8FilePath) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(100);
  header.set_file_name(std::string("hello\xffworld"));
  header.set_parent_folder(std::string());
  chunk.set_body("payload data");
  chunk.set_offset(0);
  chunk.set_flags(1);

  OfflineFrame offline_frame;
  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailedTypeFileWithNonUtf8ParentFolder) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(100);
  header.set_file_name(std::string("valid.txt"));
  header.set_parent_folder(std::string("folder\xff"));
  chunk.set_body("payload data");
  chunk.set_offset(0);
  chunk.set_flags(1);

  OfflineFrame offline_frame;
  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest, ValidatesAsFailedTypeFileWithNullInFilePath) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(100);
  header.set_file_name(std::string("hello\0world", 11));
  header.set_parent_folder(std::string());
  chunk.set_body("payload data");
  chunk.set_offset(0);
  chunk.set_flags(1);

  OfflineFrame offline_frame;
  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailedTypeFileWithNullInParentFolder) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::FILE);
  header.set_total_size(100);
  header.set_file_name(std::string("valid.txt"));
  header.set_parent_folder(std::string("folder\0name", 11));
  chunk.set_body("payload data");
  chunk.set_offset(0);
  chunk.set_flags(1);

  OfflineFrame offline_frame;
  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_EQ(ret_value.value, Exception::kIllegalCharacters);
}

TEST(OfflineFramesValidatorTest, ValidatesAsFailWithNullPayloadTransferFrame) {
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::PayloadChunk chunk;
  chunk.set_body("payload data");
  chunk.set_offset(150);
  chunk.set_flags(1);

  OfflineFrame offline_frame;

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_payload_transfer();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_payload_header();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_payload_chunk();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForDataPayloadTransfer(header, chunk);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();
  auto* payload_chunk = payload_transfer->mutable_payload_chunk();

  payload_chunk->clear_flags();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(bytes);

  auto* v1_frame = offline_frame.mutable_v1();
  auto* payload_transfer = v1_frame->mutable_payload_transfer();

  payload_transfer->clear_control_message();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
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

  std::string bytes = ForControlPayloadTransfer(header, control);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithGatewaySucceeds) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_port(kPort);
  credentials.set_frequency(kHotspotFrequency);
  credentials.set_gateway(kWifiHotspotGateway);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithAddressCandidatesSucceeds) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  auto* candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string(
      "\xfe\x80\x00\x00\x00\x00\x00\x00\x4d\xb2\xb3\x5c\x22\x03\x98\xa1", 16));
  candidate->set_port(kPort);
  candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string("\xc0\xa8\x00\x01", 4));
  candidate->set_port(kPort);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithInvlaidAddressCandidatesLengthFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  auto* candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string(
      "\xfe\x80\x00\x00\x00\x00\x00\x00\x4d\xb2\xb3\x5c\x22\x03\x98\xa1", 12));
  candidate->set_port(kPort);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithAddressCandidatesNoPortFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  auto* candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string(
      "\xfe\x80\x00\x00\x00\x00\x00\x00\x4d\xb2\xb3\x5c\x22\x03\x98\xa1", 16));
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithLargePortCandidateFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  auto* candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string("\xc0\xa8\x00\x01", 4));
  candidate->set_port(70000);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithFallbackInvalidPortFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  credentials.set_gateway(std::string(kWifiHotspotGateway));
  credentials.set_port(70000);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithEmptyGatewayAndNoCandidatesFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  credentials.set_gateway("");
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateHotspotUpgradeFrameWithNoGatewayAndNoCandidatesFails) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_frequency(kHotspotFrequency);
  // Do not set gateway
  // Do not set address candidates
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithAddressCandidatesSucceeds) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{'\x2a', '\x00', '\x79', '\xe0', '\x2e', '\x87', '\x00', '\x06', '\xb7',
        '\x28', '\x67', '\x45', '\x7a', '\xdd', '\x01', '\x53'},
       kPort},
      {{'\xc0', '\xa8', '\x00', '\x01'}, kPort},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithLoopbackAddressCandidateFails) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{127, 0, 0, 1}, kPort},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithLinkLocalAddressCandidateFails) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{169, 254, 1, 1}, kPort},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithInvalidIpAddressSizeCandidateFails) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{1, 2, 3}, kPort},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithZeroPortCandidateFails) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{192, 168, 1, 1}, 0},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithLargePortCandidateFails) {
  OfflineFrame offline_frame;
  std::vector<ServiceAddress> address_candidates = {
      {{192, 168, 1, 1}, kPort},
  };
  std::string bytes = ForBwuWifiLanPathAvailable(address_candidates);
  offline_frame.ParseFromString(bytes);

  auto* negotiation =
      offline_frame.mutable_v1()->mutable_bandwidth_upgrade_negotiation();
  auto* wifi_lan_socket =
      negotiation->mutable_upgrade_path_info()->mutable_wifi_lan_socket();
  if (wifi_lan_socket->address_candidates_size() > 0) {
    wifi_lan_socket->mutable_address_candidates(0)->set_port(70000);
  }

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithFallbackLoopbackAddressFails) {
  OfflineFrame offline_frame;
  offline_frame.set_version(OfflineFrame::V1);
  auto* v1_frame = offline_frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* negotiation = v1_frame->mutable_bandwidth_upgrade_negotiation();
  negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = negotiation->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_LAN);
  auto* wifi_lan_socket = upgrade_path_info->mutable_wifi_lan_socket();
  wifi_lan_socket->set_ip_address(std::string({127, 0, 0, 1}));
  wifi_lan_socket->set_wifi_port(kPort);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithFallbackLinkLocalAddressFails) {
  OfflineFrame offline_frame;
  offline_frame.set_version(OfflineFrame::V1);
  auto* v1_frame = offline_frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* negotiation = v1_frame->mutable_bandwidth_upgrade_negotiation();
  negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = negotiation->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_LAN);
  auto* wifi_lan_socket = upgrade_path_info->mutable_wifi_lan_socket();
  wifi_lan_socket->set_ip_address(std::string({169, 254, 1, 1}));
  wifi_lan_socket->set_wifi_port(kPort);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidateWifiLanUpgradeFrameWithFallbackInvalidPortFails) {
  OfflineFrame offline_frame;
  offline_frame.set_version(OfflineFrame::V1);
  auto* v1_frame = offline_frame.mutable_v1();
  v1_frame->set_type(V1Frame::BANDWIDTH_UPGRADE_NEGOTIATION);
  auto* negotiation = v1_frame->mutable_bandwidth_upgrade_negotiation();
  negotiation->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = negotiation->mutable_upgrade_path_info();
  upgrade_path_info->set_medium(UpgradePathInfo::WIFI_LAN);
  auto* wifi_lan_socket = upgrade_path_info->mutable_wifi_lan_socket();
  wifi_lan_socket->set_ip_address(std::string({192, 168, 1, 1}));
  wifi_lan_socket->set_wifi_port(70000);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithNullBandwidthUpgradeNegotiationFrame) {
  OfflineFrame offline_frame;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(kSsid);
  credentials.set_password(kPassword);
  credentials.set_port(kPort);
  credentials.set_frequency(kHotspotFrequency);
  credentials.set_gateway(kWifiHotspotGateway);
  std::string bytes = ForBwuWifiHotspotPathAvailable(
      std::move(credentials), kSupportsDisablingEncryption);
  offline_frame.ParseFromString(bytes);
  auto* v1_frame = offline_frame.mutable_v1();

  v1_frame->clear_bandwidth_upgrade_negotiation();

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest, ValidatesAsOkBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame;

  std::string bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), std::string(kWifiDirectPin));
  offline_frame.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame);

  EXPECT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesValidFrequencyInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  // Anything less than -1 is invalid
  std::string bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort, -2,
      kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), std::string(kWifiDirectPin));
  offline_frame_1.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  EXPECT_FALSE(ret_value.Ok());

  // But -1 itself is not invalid
  bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort, -1,
      kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), std::string(kWifiDirectPin));
  offline_frame_2.ParseFromString(bytes);

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  EXPECT_TRUE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidSsidInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  std::string wifi_direct_ssid{"DIRECT-A*-0123456789AB"};
  std::string wifi_direct_pin_wrong_length = "01234567890123456";
  std::string bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), wifi_direct_pin_wrong_length);
  offline_frame_1.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame_1);

  EXPECT_FALSE(ret_value.Ok());

  std::string wifi_direct_ssid_64_length = "DIRECT-A0-" + std::string(54, 'A');
  bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid_64_length, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), /*pin=*/"01234567890123456");
  offline_frame_2.ParseFromString(bytes);
  ret_value = EnsureValidOfflineFrame(offline_frame_2);
  EXPECT_FALSE(ret_value.Ok());

  std::string wifi_direct_pin_16_length = "0123456789012345";
  bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), wifi_direct_pin_16_length);
  offline_frame_2.ParseFromString(bytes);
  ret_value = EnsureValidOfflineFrame(offline_frame_2);
  EXPECT_TRUE(ret_value.Ok());

  std::string wifi_direct_ssid_wrong_length =
      std::string{kWifiDirectSsid} + "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
  std::string wifi_direct_device_name_wrong_length =
      std::string{kWifiDirectDeviceName} +
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789";
  bytes = ForBwuWifiDirectPathAvailable(
      wifi_direct_ssid_wrong_length, std::string(kWifiDirectPassword), kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      wifi_direct_device_name_wrong_length, std::string(kWifiDirectPin));
  offline_frame_2.ParseFromString(bytes);

  ret_value = EnsureValidOfflineFrame(offline_frame_2);

  EXPECT_FALSE(ret_value.Ok());
}

TEST(OfflineFramesValidatorTest,
     ValidatesAsFailWithInvalidPasswordInBandwidthUpgradeWifiDirect) {
  OfflineFrame offline_frame_1;
  OfflineFrame offline_frame_2;

  std::string long_wifi_direct_password =
      std::string{kWifiDirectSsid} +
      "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
  std::string long_wifi_direct_pin =
      std::string{kWifiDirectPin} +
      "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789";
  std::string bytes = ForBwuWifiDirectPathAvailable(
      std::string(kWifiDirectSsid), long_wifi_direct_password, kPort,
      kWifiDirectFrequency, kSupportsDisablingEncryption, std::string(kGateway),
      std::string(kWifiDirectDeviceName), long_wifi_direct_pin);
  offline_frame_2.ParseFromString(bytes);

  auto ret_value = EnsureValidOfflineFrame(offline_frame_2);

  EXPECT_FALSE(ret_value.Ok());
}

}  // namespace
}  // namespace nearby::connections::parser
