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

#include "internal/socket_abstraction/message_write_request.h"

#include <string>
#include <type_traits>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace socket_abstraction {
namespace {

constexpr absl::string_view kShortMessage = "short";
constexpr absl::string_view kLongMessage = "This is a long message.";
constexpr absl::string_view kLongFirstHalf = "This is a long";
constexpr absl::string_view kLongSecondHalf = " message.";

TEST(MessageWriteRequestTest, WriteRequestIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<MessageWriteRequest>::value);
}

TEST(MessageWriteRequestTest, ShortWriteRequestWorks) {
  MessageWriteRequest request =
      MessageWriteRequest(ByteArray(std::string(kShortMessage)));
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload().AsStringView(), kShortMessage);
  EXPECT_TRUE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
}

TEST(MessageWriteRequestTest, TestResetShortMessage) {
  MessageWriteRequest request =
      MessageWriteRequest(ByteArray(std::string(kShortMessage)));
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload().AsStringView(), kShortMessage);
  EXPECT_TRUE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  request.Reset();
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  EXPECT_EQ(request.NextPacket(15).value().GetPayload().AsStringView(),
            kShortMessage);
}

TEST(MessageWriteRequestTest, LongWriteRequestWorks) {
  MessageWriteRequest request =
      MessageWriteRequest(ByteArray(std::string(kLongMessage)));
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload().AsStringView(), kLongFirstHalf);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  Packet nextPacket = request.NextPacket(15).value();
  EXPECT_TRUE(nextPacket.IsDataPacket());
  EXPECT_FALSE(nextPacket.IsFirstPacket());
  EXPECT_TRUE(nextPacket.IsLastPacket());
  EXPECT_EQ(nextPacket.GetPayload().AsStringView(), kLongSecondHalf);
  EXPECT_TRUE(request.IsFinished());
}

TEST(MessageWriteRequestTest, TestResetLongRequest) {
  MessageWriteRequest request =
      MessageWriteRequest(ByteArray(std::string(kLongMessage)));
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload().AsStringView(), kLongFirstHalf);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  // Resetting in the middle of the message.
  request.Reset();
  EXPECT_FALSE(request.IsStarted());
  EXPECT_FALSE(request.IsFinished());
  Packet nextPacket = request.NextPacket(15).value();
  EXPECT_TRUE(nextPacket.IsDataPacket());
  EXPECT_TRUE(nextPacket.IsFirstPacket());
  EXPECT_FALSE(nextPacket.IsLastPacket());
  EXPECT_EQ(nextPacket.GetPayload().AsStringView(), kLongFirstHalf);
  EXPECT_FALSE(request.IsFinished());
  // Send the full message, then resetting at the end.
  Packet lastPacket = request.NextPacket(15).value();
  EXPECT_TRUE(nextPacket.IsDataPacket());
  EXPECT_FALSE(lastPacket.IsFirstPacket());
  EXPECT_TRUE(lastPacket.IsLastPacket());
  EXPECT_EQ(lastPacket.GetPayload().AsStringView(), kLongSecondHalf);
  EXPECT_TRUE(request.IsFinished());
  request.Reset();
  EXPECT_FALSE(request.IsStarted());
  EXPECT_FALSE(request.IsFinished());
}

TEST(MessageWriteRequesttest, TestResourceExhaustionOnceMessageSent) {
  MessageWriteRequest request =
      MessageWriteRequest(ByteArray(std::string(kShortMessage)));
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload().AsStringView(), kShortMessage);
  EXPECT_TRUE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  EXPECT_THAT(request.NextPacket(15),
              testing::status::StatusIs(absl::StatusCode::kResourceExhausted));
}

}  // namespace
}  // namespace socket_abstraction
}  // namespace nearby
