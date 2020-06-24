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

#include "platform_v2/base/byte_array.h"

#include <cstring>

#include "gtest/gtest.h"

namespace {

using location::nearby::ByteArray;

TEST(ByteArrayTest, DefaultSizeIsZero) {
  ByteArray bytes;
  EXPECT_EQ(0, bytes.size());
}

TEST(ByteArrayTest, DefaultIsEmpty) {
  ByteArray bytes;
  EXPECT_TRUE(bytes.Empty());
}

TEST(ByteArrayTest, NullArrayIsEmpty) {
  ByteArray bytes{nullptr, 5};
  EXPECT_TRUE(bytes.Empty());
}

TEST(ByteArrayTest, CopyAtDoesNotExtendArray) {
  ByteArray v1("12345");
  ByteArray v2("ABCDEFGH");
  EXPECT_TRUE(v2.CopyAt(/*offset=*/5, v1));
  EXPECT_TRUE(v2.CopyAt(/*offset=*/1, v1, /*source_offset=*/3));
  EXPECT_EQ(v2, ByteArray("A45DE123"));
}

TEST(ByteArrayTest, CopyAtOutOfBoundsIsIgnored) {
  ByteArray v1("12345");
  ByteArray v2("ABCDEFGH");
  // Try to do an out-of-bounds read.
  EXPECT_FALSE(v2.CopyAt(/* offset=*/5, v1, /*source_offset=*/10));
  // Try to do an out-of-bounds write.
  EXPECT_FALSE(v2.CopyAt(/* offset=*/9, v1));
  EXPECT_EQ(v2, ByteArray("ABCDEFGH"));
}

TEST(ByteArrayTest, SetFromString) {
  std::string setup("setup_test");
  ByteArray bytes{setup};  // array initialized with a copy of string.
  EXPECT_EQ(setup.size(), bytes.size());
  EXPECT_EQ(std::string(bytes), setup);
}

TEST(ByteArrayTest, SetExplicitSize) {
  constexpr size_t kArraySize = 10;
  char reference[kArraySize]{};
  ByteArray bytes{kArraySize};  // array of size 10, zero-initialized.
  EXPECT_EQ(kArraySize, bytes.size());
  EXPECT_EQ(0, memcmp(bytes.data(), reference, kArraySize));
}

TEST(ByteArrayTest, SetExplicitData) {
  constexpr static const char message[]{"test_message"};
  constexpr size_t kMessageSize = sizeof(message);
  ByteArray bytes{message, kMessageSize};
  EXPECT_EQ(kMessageSize, bytes.size());
  EXPECT_NE(message, bytes.data());
  EXPECT_EQ(0, memcmp(message, bytes.data(), kMessageSize));
}

TEST(ByteArrayTest, CreateFromNonNullTerminatedStdArray) {
  constexpr static const std::array data{'a', '\x00', 'b'};
  ByteArray bytes{data};
  EXPECT_EQ(bytes.size(), 3);
  EXPECT_EQ(bytes.size(), std::string(bytes).size());
  EXPECT_EQ(std::string(bytes), std::string(data.data(), data.size()));
}

}  // namespace
