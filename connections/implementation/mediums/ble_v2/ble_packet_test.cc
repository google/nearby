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

#include "connections/implementation/mediums/ble_v2/ble_packet.h"

#include <algorithm>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "proto/mediums/ble_frames.proto.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {
using ::location::nearby::mediums::SocketControlFrame;
using ::protobuf_matchers::EqualsProto;

constexpr absl::string_view kServiceIDHash = {"\x01\x02\x03"};
constexpr absl::string_view kData = {"\x01\x02\x03\x04\x05"};

TEST(BlePacketTest, CreatingControlPacketWorks) {
  ByteArray data((std::string(kData)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlPacket(data);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());
  EXPECT_EQ(data, ble_packet_status_or.value().GetData());
}

TEST(BlePacketTest, CreatingControlIntroductionFramePacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlIntroductionPacket(service_id_hash);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());

  constexpr absl::string_view kExpected =
      R"pb(
    type: INTRODUCTION
    introduction: < service_id_hash: "\001\002\003" socket_version: V2 >)pb";

  SocketControlFrame frame;
  frame.ParseFromString(std::string(ble_packet_status_or.value().GetData()));

  EXPECT_THAT(frame, EqualsProto(kExpected));
}

TEST(BlePacketTest, CreatingControlDisconnectionFramePacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlDisconnectionPacket(service_id_hash);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());

  constexpr absl::string_view kExpected =
      R"pb(
    type: DISCONNECTION
    disconnection: < service_id_hash: "\001\002\003" >)pb";

  SocketControlFrame frame;
  frame.ParseFromString(std::string(ble_packet_status_or.value().GetData()));

  EXPECT_THAT(frame, EqualsProto(kExpected));
}

TEST(BlePacketTest, CreatingControlPacketAcknowledgementFramePacketWorks) {
  ByteArray service_id_hash((std::string(kServiceIDHash)));

  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlPacketAcknowledgementPacket(service_id_hash, 100);

  ASSERT_OK(ble_packet_status_or);
  EXPECT_TRUE(ble_packet_status_or.value().IsValid());
  EXPECT_TRUE(ble_packet_status_or.value().IsControlPacket());

  constexpr absl::string_view kExpected =
      R"pb(
    type: PACKET_ACKNOWLEDGEMENT
    packet_acknowledgement: <
      service_id_hash: "\001\002\003"
      received_size: 100
    >)pb";

  SocketControlFrame frame;
  frame.ParseFromString(std::string(ble_packet_status_or.value().GetData()));

  EXPECT_THAT(frame, EqualsProto(kExpected));
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

TEST(BlePacketTest,
     ConstructionFromSerializedBytesAsDataByteWithInvalidControlBytes) {
  ByteArray data((std::string(kData)));

  // Construct a control packet.
  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlPacket(data);
  ByteArray ble_packet_bytes(ble_packet_status_or.value());

  // Corrupt the first byte of ble_packet_bytes.
  memset(ble_packet_bytes.data(), 0x01, 1);

  BlePacket new_ble_packet(ble_packet_bytes);

  EXPECT_TRUE(new_ble_packet.IsValid());
  // It is not control packet eventually.
  EXPECT_FALSE(new_ble_packet.IsControlPacket());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
