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

#include "internal/weave/control_packet_write_request.h"

#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {
namespace {

TEST(ControlPacketWriteRequestTest, WriteRequestIsNotTriviallyConstructible) {
  EXPECT_FALSE(
      std::is_trivially_constructible<ControlPacketWriteRequest>::value);
}

TEST(ControlPacketWriteRequestTest, WriteRequestWorks) {
  Packet packet = Packet::CreateConnectionRequestPacket(1, 1, 15, "").value();
  std::string packet_bytes = packet.GetBytes();
  auto request = ControlPacketWriteRequest(std::move(packet));
  EXPECT_EQ(packet_bytes, request.NextPacket(15).value().GetBytes());
}

TEST(ControlPacketWriteRequestTest, TestOutOfRange) {
  Packet packet = Packet::CreateConnectionRequestPacket(1, 1, 15, "").value();
  std::string packet_bytes = packet.GetBytes();
  auto request = ControlPacketWriteRequest(std::move(packet));
  EXPECT_EQ(packet_bytes, request.NextPacket(15).value().GetBytes());
  EXPECT_THAT(request.NextPacket(15),
              testing::status::StatusIs(absl::StatusCode::kOutOfRange));
}

}  // namespace
}  // namespace weave
}  // namespace nearby
