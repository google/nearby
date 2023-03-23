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

#include "internal/weave/packetizer.h"

#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {
namespace {

using ::testing::status::StatusIs;

TEST(PacketizerTest, TestAddPacket) {
  Packet packet1 = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet1.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(
      /*is_first_packet=*/false, /*is_last_packet=*/true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));

  Packetizer packetizer;
  EXPECT_OK(packetizer.AddPacket(std::move(packet1)));
  EXPECT_THAT(packetizer.TakeMessage(),
              StatusIs(absl::StatusCode::kUnavailable));
  EXPECT_OK(packetizer.AddPacket(std::move(packet2)));
  auto message = packetizer.TakeMessage();
  ASSERT_OK(message);
  EXPECT_EQ(message->string_data(), "helloworld");
}

TEST(PacketizerTest, TestAddPacketTwice) {
  Packet packet1 = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet1.SetPacketCounter(1));
  Packet packet1_copy = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet1_copy.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(
      /*is_first_packet=*/false, /*is_last_packet=*/true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));
  Packet packet2_copy = Packet::CreateDataPacket(
      /*is_first_packet=*/false, /*is_last_packet=*/true, ByteArray("world"));
  EXPECT_OK(packet2_copy.SetPacketCounter(2));

  Packetizer packetizer;
  EXPECT_OK(packetizer.AddPacket(std::move(packet1)));
  EXPECT_OK(packetizer.AddPacket(std::move(packet2)));
  absl::StatusOr<ByteArray> message = packetizer.TakeMessage();
  ASSERT_OK(message);
  EXPECT_EQ(message->string_data(), "helloworld");

  EXPECT_OK(packetizer.AddPacket(std::move(packet1_copy)));
  EXPECT_OK(packetizer.AddPacket(std::move(packet2_copy)));
  message = packetizer.TakeMessage();
  EXPECT_OK(message);
  // Make sure it isn't helloworldhelloworld.
  EXPECT_EQ(message->string_data(), "helloworld");
}

TEST(PacketizerTest, TestReset) {
  Packet packet1 = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet1.SetPacketCounter(1));
  Packet packet1_copy = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet1_copy.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(
      /*is_first_packet=*/false, /*is_last_packet=*/true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));
  Packet packet2_copy = Packet::CreateDataPacket(
      /*is_first_packet=*/false, /*is_last_packet=*/true, ByteArray("world"));
  EXPECT_OK(packet2_copy.SetPacketCounter(2));

  Packetizer packetizer;
  EXPECT_OK(packetizer.AddPacket(std::move(packet1)));
  packetizer.Reset();
  EXPECT_THAT(packetizer.AddPacket(std::move(packet2)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_OK(packetizer.AddPacket(std::move(packet1_copy)));
  EXPECT_THAT(packetizer.TakeMessage(),
              StatusIs(absl::StatusCode::kUnavailable));
  EXPECT_OK(packetizer.AddPacket(std::move(packet2_copy)));
  absl::StatusOr<ByteArray> message = packetizer.TakeMessage();
  EXPECT_OK(message);
  EXPECT_EQ(message.value(), ByteArray("helloworld"));
}

TEST(PacketizerTest, TestAddTwoFirstPacketsFails) {
  Packet packet = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  Packet copy = Packet::CreateDataPacket(
      /*is_first_packet=*/true, /*is_last_packet=*/false, ByteArray("hello"));
  EXPECT_OK(packet.SetPacketCounter(1));

  Packetizer packetizer;
  EXPECT_OK(packetizer.AddPacket(std::move(packet)));
  EXPECT_THAT(packetizer.AddPacket(std::move(copy)),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace weave
}  // namespace nearby
