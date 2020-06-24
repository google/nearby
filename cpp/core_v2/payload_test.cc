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

#include "core_v2/payload.h"

#include <memory>
#include <type_traits>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/public/file.h"
#include "platform_v2/public/pipe.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {

TEST(PayloadTest, DefaultPayloadHasUnknownType) {
  Payload payload;
  EXPECT_EQ(payload.GetType(), Payload::Type::kUnknown);
}

TEST(PayloadTest, SupportsByteArrayType) {
  const ByteArray bytes("bytes");
  Payload payload(bytes);
  EXPECT_EQ(payload.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), bytes);
}

TEST(PayloadTest, SupportsFileType) {
  const auto payload_id = Payload::GenerateId();
  InputFile file(payload_id, 100);
  InputStream& stream = file.GetInputStream();
  Payload payload(payload_id, std::move(file));
  EXPECT_EQ(payload.GetType(), Payload::Type::kFile);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(&payload.AsFile()->GetInputStream(), &stream);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
}

TEST(PayloadTest, SupportsStreamType) {
  auto pipe = std::make_shared<Pipe>();
  Payload payload(
      [streamable = pipe]() -> InputStream& {
        // For some reason, linter warns us that we return a dangling reference.
        // This is not true: we return a reference to internal variable of a
        // shared_ptr<Pipe> which remains valid while Payload is valid, since
        // shared_ptr<Pipe> is captured by value.
        return streamable->GetInputStream();  // NOLINT
      });
  EXPECT_EQ(payload.GetType(), Payload::Type::kStream);
  EXPECT_EQ(payload.AsStream(), &pipe->GetInputStream());
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
}

TEST(PayloadTest, PayloadIsMoveable) {
  Payload payload1;
  Payload payload2(ByteArray("bytes"));
  auto id = payload2.GetId();
  ByteArray bytes = payload2.AsBytes();
  EXPECT_EQ(payload1.GetType(), Payload::Type::kUnknown);
  EXPECT_EQ(payload2.GetType(), Payload::Type::kBytes);
  payload1 = std::move(payload2);
  EXPECT_EQ(payload1.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload1.AsBytes(), bytes);
  EXPECT_EQ(payload1.GetId(), id);
}

TEST(PayloadTest, PayloadHasUniqueId) {
  Payload payload1;
  Payload payload2;
  EXPECT_NE(payload1.GetId(), payload2.GetId());
}

TEST(PayloadTest, PayloadIsNotCopyable) {
  EXPECT_FALSE(std::is_copy_constructible_v<Payload>);
  EXPECT_FALSE(std::is_copy_assignable_v<Payload>);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
