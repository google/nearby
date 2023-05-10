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

#include "internal/weave/message_write_request.h"

#include <string>
#include <type_traits>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/future.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {
namespace {

constexpr absl::string_view kShortMessage = "short";
constexpr absl::string_view kLongMessage = "This is a long message.";
constexpr absl::string_view kLongFirstHalf = "This is a long";
constexpr absl::string_view kLongSecondHalf = " message.";

TEST(MessageWriteRequestTest, WriteRequestIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<MessageWriteRequest>::value);
}

TEST(MessageWriteRequestTest, ShortWriteRequestWorks) {
  MessageWriteRequest request = MessageWriteRequest(kShortMessage);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), kShortMessage);
  EXPECT_TRUE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
}

TEST(MessageWriteRequestTest, LongWriteRequestWorks) {
  MessageWriteRequest request = MessageWriteRequest(kLongMessage);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_FALSE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), kLongFirstHalf);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  Packet nextPacket = request.NextPacket(15).value();
  EXPECT_TRUE(nextPacket.IsDataPacket());
  EXPECT_FALSE(nextPacket.IsFirstPacket());
  EXPECT_TRUE(nextPacket.IsLastPacket());
  EXPECT_EQ(nextPacket.GetPayload(), kLongSecondHalf);
  EXPECT_TRUE(request.IsFinished());
}

TEST(MessageWriteRequestTest, TestResourceExhaustionOnceMessageSent) {
  MessageWriteRequest request = MessageWriteRequest(kShortMessage);
  EXPECT_FALSE(request.IsFinished());
  EXPECT_FALSE(request.IsStarted());
  Packet packet = request.NextPacket(15).value();
  EXPECT_TRUE(packet.IsDataPacket());
  EXPECT_TRUE(packet.IsFirstPacket());
  EXPECT_TRUE(packet.IsLastPacket());
  EXPECT_EQ(packet.GetPayload(), kShortMessage);
  EXPECT_TRUE(request.IsFinished());
  EXPECT_TRUE(request.IsStarted());
  EXPECT_THAT(request.NextPacket(15),
              testing::status::StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(MessageWriteRequestTest, TestGetSetFuture) {
  MessageWriteRequest request = MessageWriteRequest(kShortMessage);
  nearby::Future<absl::Status> result = request.GetWriteStatusFuture();
  request.SetWriteStatus(absl::InternalError(""));
  EXPECT_THAT(result.Get().GetResult(),
              testing::status::StatusIs(absl::StatusCode::kInternal));
}

TEST(MessageWriteRequestTest, TestInvalidPacketSize) {
  MessageWriteRequest request = MessageWriteRequest(kShortMessage);
  auto result = request.NextPacket(0);
  EXPECT_THAT(result,
              testing::status::StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace weave
}  // namespace nearby
