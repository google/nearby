// Copyright 2023 Google LLC
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

#include "internal/weave/packet.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace nearby {
namespace weave {

namespace {

constexpr absl::string_view kExtraData = "\x34\x56\x78";
constexpr absl::string_view kConnectionRequestExtraData = "thirteen char";
constexpr absl::string_view kConnectionConfirmExtraData = "fifteen char...";

TEST(PacketTest, PacketIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<Packet>());
}

TEST(PacketTest, CreateErrorPacketTest) {
  Packet packet = Packet::CreateErrorPacket();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::ControlPacketType::kControlError);
  EXPECT_EQ(packet.GetPayload().size(), 0);
}

TEST(PacketTest, CreateConnectionConfirmPacketGoodTest) {
  Packet packet =
      Packet::CreateConnectionConfirmPacket(1, 15, kConnectionConfirmExtraData)
          .value();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionConfirm);
  ASSERT_EQ(packet.GetBytes().size(), 20);
  auto expected = absl::StrFormat("%c%c%c%c%c%s", 0b10000001, 0x00, 0x01, 0x00,
                                  0x0F, kConnectionConfirmExtraData);
  EXPECT_EQ(packet.GetBytes(), expected);
}

TEST(PacketTest, CreateConnectionConfirmPacketBadTest) {
  EXPECT_FALSE(Packet::CreateConnectionConfirmPacket(
                   /*selected_protocol_version=*/1, /*selected_packet_size=*/15,
                   "> than fifteen..")
                   .ok());
}

TEST(PacketTest, CreateConnectionRequestGoodPacketTest) {
  Packet packet = Packet::CreateConnectionRequestPacket(
                      0x0001, 0x0001, 0x000F, kConnectionRequestExtraData)
                      .value();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionRequest);
  ASSERT_EQ(packet.GetPayload().size(), 19);
  ASSERT_EQ(packet.GetBytes().size(), 20);
  auto expected = absl::StrCat(
      absl::StrFormat("%c%c%c%c%c%c%c%s", 0b10000000, 0x00, 0x01, 0x00, 0x01,
                      0x00, 0x0F, kConnectionRequestExtraData));
  EXPECT_EQ(packet.GetBytes(), expected);
}

TEST(PacketTest, CreateConnectionRequestBigProtocolVersionPacketTest) {
  ByteArray extraData = ByteArray(std::string(kExtraData));
  Packet packet =
      Packet::CreateConnectionRequestPacket(0x1001, 0x1001, 0x000F, kExtraData)
          .value();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionRequest);
  ASSERT_EQ(packet.GetPayload().size(), 9);
  ASSERT_EQ(packet.GetBytes().size(), 10);
  auto expected =
      absl::StrCat(absl::StrFormat("%c%c%c%c%c%c%c%s", 0b10000000, 0x10, 0x01,
                                   0x10, 0x01, 0x00, 0x0F, kExtraData));
  EXPECT_EQ(packet.GetBytes(), expected);
}

TEST(PacketTest, CreateConnectionRequestBadPacketTest) {
  EXPECT_FALSE(
      Packet::CreateConnectionRequestPacket(1, 1, 15, "> than 13 chr.").ok());
}

TEST(PacketTest, CreateDataPacketTest) {
  Packet packet =
      Packet::CreateDataPacket(false, false, ByteArray("big payload"));
  ASSERT_EQ(packet.GetBytes().size(), 12);
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_FALSE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), "big payload");
  EXPECT_EQ(packet.GetBytes().substr(1), "big payload");
  EXPECT_EQ(packet.GetPacketCounter(), 0);
}

TEST(PacketTest, SetPacketCounterTest) {
  Packet packet = Packet::CreateDataPacket(false, false, ByteArray("sample"));
  EXPECT_OK(packet.SetPacketCounter(1));
  EXPECT_EQ(packet.GetPacketCounter(), 1);
  EXPECT_THAT(packet.SetPacketCounter(Packet::kMaxPacketCounter + 1),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(packet.SetPacketCounter(-1),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(packet.SetPacketCounter(8),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PacketTest, StringifyTest) {
  Packet packet = Packet::CreateDataPacket(false, false, ByteArray("sample"));
  EXPECT_EQ(packet.ToString(),
            absl::StrFormat("Packet[header: 0b%08d + payload: %d bytes]",
                            0b00000000, 6));
}

TEST(PacketTest, CreateFirstPacketTest) {
  Packet packet = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("sample"));
  EXPECT_FALSE(packet.IsControlPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), "sample");
}

TEST(PacketTest, CreateLastPacketTest) {
  Packet packet = Packet::CreateDataPacket(false, true, ByteArray("sample"));
  EXPECT_FALSE(packet.IsControlPacket());
  EXPECT_FALSE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), "sample");
}

}  // namespace

}  // namespace weave
}  // namespace nearby
