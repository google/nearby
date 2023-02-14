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

#include "internal/socket_abstraction/packet.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace nearby {
namespace socket_abstraction {

namespace {

TEST(PacketTest, PacketIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<Packet>());
}

TEST(PacketTest, CreateErrorPacketTest) {
  Packet packet = Packet::CreateErrorPacket();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(), Packet::kControlError);
  EXPECT_EQ(packet.GetPayload().size(), 0);
}

constexpr absl::string_view kExtraData = "\x34\x56\x78";
TEST(PacketTest, CreateConnectionConfirmPacketGoodTest) {
  ByteArray extraData = ByteArray(std::string(kExtraData));
  Packet packet =
      Packet::CreateConnectionConfirmPacket(1, 15, extraData).value();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::kControlConnectionConfirm);
  EXPECT_EQ(packet.GetPayload().data()[1] << 8 | packet.GetPayload().data()[0],
            1);
  EXPECT_EQ(packet.GetPayload().data()[3] << 8 | packet.GetPayload().data()[2],
            15);
  EXPECT_EQ(packet.GetBytes().size(), 8);
  EXPECT_EQ(packet.GetPayload().size(), 7);
  auto expected = absl::StrCat(absl::StrFormat("%c", 0b10000001),
                               packet.GetPayload().AsStringView());
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], packet.GetBytes().data()[i]);
  }
}

TEST(PacketTest, CreateConnectionConfirmPacketBadTest) {
  bool ok = Packet::CreateConnectionConfirmPacket(
                1, 15, ByteArray("more than fifteen characters"))
                .ok();
  EXPECT_FALSE(ok);
}

TEST(PacketTest, CreateConnectionRequestGoodPacketTest) {
  ByteArray extraData = ByteArray("hello world");
  Packet packet =
      Packet::CreateConnectionRequestPacket(0b0001, 0b0001, 0x000F, extraData)
          .value();
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::kControlConnectionRequest);
  EXPECT_EQ(packet.GetPayload().data()[1] << 8 | packet.GetPayload().data()[0],
            1);
  EXPECT_EQ(packet.GetPayload().data()[3] << 8 | packet.GetPayload().data()[2],
            1);
  EXPECT_EQ(packet.GetPayload().data()[5] << 8 | packet.GetPayload().data()[4],
            15);
  EXPECT_EQ(packet.GetPayload().size(), 17);
  auto expected = absl::StrCat(absl::StrFormat("%c", 0b10000000),
                               packet.GetPayload().AsStringView());
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], packet.GetBytes().data()[i]);
  }
}

TEST(PacketTest, CreateConnectionRequestBadPacketTest) {
  bool ok = Packet::CreateConnectionRequestPacket(
                1, 1, 15, ByteArray("more than thirteen characters"))
                .ok();
  EXPECT_FALSE(ok);
}

TEST(PacketTest, CreateDataPacketTest) {
  Packet packet =
      Packet::CreateDataPacket(false, false, ByteArray("big payload"));
  EXPECT_FALSE(packet.IsControlPacket());
  EXPECT_FALSE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), ByteArray("big payload"));
}

TEST(PacketTest, SetPacketCounterTest) {
  Packet packet = Packet::CreateDataPacket(false, false, ByteArray("sample"));
  EXPECT_OK(packet.SetPacketCounter(1));
  EXPECT_EQ(packet.GetPacketCounter(), 1);
  EXPECT_THAT(packet.SetPacketCounter(Packet::kMaxPacketCounter + 1),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(PacketTest, StringifyTest) {
  Packet packet = Packet::CreateDataPacket(false, false, ByteArray("sample"));
  EXPECT_EQ(packet.ToString(),
            absl::StrFormat("Packet[%08d + %d bytes payload]", 0b00000000, 6));
}

TEST(PacketTest, CreateFirstPacketTest) {
  Packet packet = Packet::CreateDataPacket(true, false, ByteArray("sample"));
  EXPECT_FALSE(packet.IsControlPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), ByteArray("sample"));
}

TEST(PacketTest, CreateLastPacketTest) {
  Packet packet = Packet::CreateDataPacket(false, true, ByteArray("sample"));
  EXPECT_FALSE(packet.IsControlPacket());
  EXPECT_FALSE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), ByteArray("sample"));
}

}  // namespace

}  // namespace socket_abstraction
}  // namespace nearby
