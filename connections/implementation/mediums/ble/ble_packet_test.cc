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

#include "connections/implementation/mediums/ble/ble_packet.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {
using ::location::nearby::mediums::SocketControlFrame;
using ::location::nearby::mediums::SocketVersion;

constexpr absl::string_view kServiceIDHash = {"\x01\x02\x03"};
constexpr absl::string_view kData = {"\x01\x02\x03\x04\x05"};

TEST(BlePacketTest, CreatingControlIntroductionFramePacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlIntroductionPacket(service_id_hash);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());
  EXPECT_EQ(ble_packet_status_or.value().GetControlFrameType(),
            SocketControlFrame::INTRODUCTION);
  ASSERT_OK_AND_ASSIGN(
      SocketVersion version,
      ble_packet_status_or.value().GetIntroductonSocketVersion());
  EXPECT_EQ(version, SocketVersion::V2);
  EXPECT_EQ(ble_packet_status_or.value().GetServiceIdHash(), service_id_hash);
}

TEST(BlePacketTest, CreatingControlDisconnectionFramePacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlDisconnectionPacket(service_id_hash);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());
  EXPECT_EQ(ble_packet_status_or.value().GetControlFrameType(),
            SocketControlFrame::DISCONNECTION);
  EXPECT_EQ(ble_packet_status_or.value().GetServiceIdHash(), service_id_hash);
}

TEST(BlePacketTest, CreatingControlPacketAcknowledgementFramePacketWorks) {
  constexpr int kReceivedSize = 100;
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlPacketAcknowledgementPacket(service_id_hash,
                                                          kReceivedSize);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());
  EXPECT_EQ(ble_packet_status_or.value().GetControlFrameType(),
            SocketControlFrame::PACKET_ACKNOWLEDGEMENT);
  EXPECT_EQ(ble_packet_status_or.value().GetServiceIdHash(), service_id_hash);
  ASSERT_OK_AND_ASSIGN(
      int received_size,
      ble_packet_status_or.value().GetPacketAcknowledgementReceivedSize());
  EXPECT_EQ(received_size, kReceivedSize);
}

TEST(BlePacketTest, CreatingDataPacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_EQ(service_id_hash, ble_packet_status_or.value().GetServiceIdHash());
  EXPECT_EQ(data, ble_packet_status_or.value().GetData());
}

TEST(BlePacketTest, CreatingDataPacketWorksWithEmptyData) {
  char empty_data[] = "";

  ByteArray service_id_hash((std::string(kServiceIDHash)));
  ByteArray data(empty_data);

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_EQ(service_id_hash, ble_packet_status_or.value().GetServiceIdHash());
  EXPECT_EQ(data, ble_packet_status_or.value().GetData());
}

TEST(BlePacketTest, CreatingDataPacketFailsWithShortServiceIdHash) {
  char short_service_id_hash[] = "\x0a\x0b";

  ByteArray service_id_hash(short_service_id_hash);
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  EXPECT_FALSE(ble_packet_status_or.ok());
}

TEST(BlePacketTest, CreatingDataPacketFailsWithLongServiceIdHash) {
  char long_service_id_hash[] = "\x0a\x0b\x0c\x0d";

  ByteArray service_id_hash(long_service_id_hash);
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  EXPECT_FALSE(ble_packet_status_or.ok());
}

TEST(BlePacketTest, ConstructionFromSerializedBytesWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> org_ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  ASSERT_OK(org_ble_packet_status_or);

  ByteArray ble_packet_bytes(org_ble_packet_status_or.value());

  BlePacket ble_packet(ble_packet_bytes);

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionFromSerializedShortLengthDataBytesFails) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> org_ble_packet_status_or =
      BlePacket::CreateDataPacket(service_id_hash, data);

  ASSERT_OK(org_ble_packet_status_or);

  ByteArray org_ble_packet_bytes(org_ble_packet_status_or.value());

  // Cut off the packet so that it's too short
  ByteArray short_ble_packet_bytes(ByteArray(org_ble_packet_bytes.data(), 2));

  BlePacket short_ble_packet(short_ble_packet_bytes);

  EXPECT_FALSE(short_ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromEmptyBytesIsInvalid) {
  BlePacket ble_packet((ByteArray()));
  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromSerializedControlBytesWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHash)};
  absl::StatusOr<BlePacket> org_ble_packet_status_or =
      BlePacket::CreateControlIntroductionPacket(service_id_hash);
  ASSERT_OK(org_ble_packet_status_or);

  ByteArray ble_packet_bytes(org_ble_packet_status_or.value());
  BlePacket ble_packet(ble_packet_bytes);

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_TRUE(ble_packet.IsControlPacket());
  EXPECT_EQ(ble_packet.GetServiceIdHash(), service_id_hash);
  EXPECT_EQ(ble_packet.GetControlFrameType(),
            SocketControlFrame::INTRODUCTION);
  ASSERT_OK_AND_ASSIGN(
      SocketVersion version,
      ble_packet.GetIntroductonSocketVersion());
  EXPECT_EQ(version, SocketVersion::V2);
}

TEST(BlePacketTest, ConstructionFromInvalidControlPacketIsInvalid) {
  ByteArray invalid_control_packet_bytes("\x00\x00\x00\x01\x02\x03", 6);
  BlePacket ble_packet(invalid_control_packet_bytes);
  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, IsControlPacketBytes) {
  EXPECT_TRUE(BlePacket::IsControlPacketBytes(ByteArray("\x00\x00\x00", 3)));
  EXPECT_FALSE(
      BlePacket::IsControlPacketBytes(ByteArray(std::string(kServiceIDHash))));
  EXPECT_FALSE(BlePacket::IsControlPacketBytes(ByteArray("invalid", 7)));
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
