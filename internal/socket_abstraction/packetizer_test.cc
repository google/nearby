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

#include "internal/socket_abstraction/packetizer.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "internal/socket_abstraction/packet.h"

namespace nearby {
namespace socket_abstraction {
namespace {

TEST(PacketizerTest, TestJoin) {
  Packet packet = Packet::CreateDataPacket(true, false, ByteArray("hello"));
  EXPECT_OK(packet.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(false, true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));
  Packetizer packetizer;
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
  EXPECT_OK(packetizer.Join(packet2));
}

TEST(PacketizerTest, TestJoinTwice) {
  Packet packet = Packet::CreateDataPacket(true, false, ByteArray("hello"));
  EXPECT_OK(packet.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(false, true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));
  Packetizer packetizer;
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
  absl::StatusOr<ByteArray> joined = packetizer.Join(packet2);
  EXPECT_OK(joined);
  EXPECT_EQ(joined.value(), ByteArray("helloworld"));
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
  joined = packetizer.Join(packet2);
  EXPECT_OK(joined);
  // make sure it isn't helloworldhelloworld
  EXPECT_EQ(joined.value(), ByteArray("helloworld"));
}

TEST(PacketizerTest, TestReset) {
  Packet packet = Packet::CreateDataPacket(true, false, ByteArray("hello"));
  EXPECT_OK(packet.SetPacketCounter(1));
  Packet packet2 = Packet::CreateDataPacket(false, true, ByteArray("world"));
  EXPECT_OK(packet2.SetPacketCounter(2));
  Packetizer packetizer;
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
  packetizer.Reset();
  EXPECT_THAT(packetizer.Join(packet2),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
}

TEST(PacketizerTest, AddTwoFirstPacketsFails) {
  Packet packet = Packet::CreateDataPacket(true, false, ByteArray("hello"));
  EXPECT_OK(packet.SetPacketCounter(1));
  Packetizer packetizer;
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kUnavailable));
  EXPECT_THAT(packetizer.Join(packet),
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace socket_abstraction
}  // namespace nearby
