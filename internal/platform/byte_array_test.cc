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

#include "internal/platform/byte_array.h"

#include <stddef.h>

#include <array>
#include <concepts>
#include <cstring>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"

namespace {

using ::nearby::ByteArray;

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
  EXPECT_EQ(bytes.string_data(), setup);
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

TEST(ByteArrayTest, CreateFromAbslStringReturnsTheSame) {
  const absl::string_view kTestString = "Test String";
  ByteArray bytes{std::string(kTestString)};

  EXPECT_EQ(bytes.size(), kTestString.size());
  EXPECT_EQ(bytes.AsStringView(), kTestString);
}

TEST(ByteArrayTest, IteratorTypes) {
  static_assert(std::same_as<decltype(std::declval<ByteArray>().begin()),
                             ByteArray::iterator>);
  static_assert(std::same_as<decltype(std::declval<ByteArray>().cbegin()),
                             ByteArray::const_iterator>);
  static_assert(std::same_as<decltype(std::declval<ByteArray>().end()),
                             ByteArray::iterator>);
  static_assert(std::same_as<decltype(std::declval<ByteArray>().cend()),
                             ByteArray::const_iterator>);

  static_assert(std::same_as<decltype(std::declval<const ByteArray>().begin()),
                             ByteArray::const_iterator>);
  static_assert(std::same_as<decltype(std::declval<const ByteArray>().end()),
                             ByteArray::const_iterator>);
}

TEST(ByteArrayTest, Iterators) {
  ByteArray bytes("12345");
  const ByteArray const_bytes("12345");
  static constexpr auto kExpected = std::to_array({'1', '2', '3', '4', '5'});

  // Check manual iteration.
  {
    size_t i = 0;
    for (auto it = bytes.begin(); it != bytes.end(); ++it) {
      ASSERT_LT(i, kExpected.size());
      EXPECT_EQ(*it, kExpected[i++]);
    }
    EXPECT_EQ(i, kExpected.size());
  }
  {
    size_t i = 0;
    for (auto it = bytes.cbegin(); it != bytes.cend(); ++it) {
      ASSERT_LT(i, kExpected.size());
      EXPECT_EQ(*it, kExpected[i++]);
    }
    EXPECT_EQ(i, kExpected.size());
  }
  {
    size_t i = 0;
    for (auto it = const_bytes.begin(); it != const_bytes.end(); ++it) {
      ASSERT_LT(i, kExpected.size());
      EXPECT_EQ(*it, kExpected[i++]);
    }
    EXPECT_EQ(i, kExpected.size());
  }

  // Check range-for loops.
  {
    size_t i = 0;
    for (auto c : bytes) {
      ASSERT_LT(i, kExpected.size());
      EXPECT_EQ(c, kExpected[i++]);
    }
    EXPECT_EQ(i, kExpected.size());
  }
  {
    size_t i = 0;
    for (auto c : const_bytes) {
      ASSERT_LT(i, kExpected.size());
      EXPECT_EQ(c, kExpected[i++]);
    }
    EXPECT_EQ(i, kExpected.size());
  }
}

TEST(ByteArrayTest, EmptyArrayIterators) {
  // It should be legal to call begin()/end() etc. on empty arrays.
  ByteArray bytes;
  const ByteArray const_bytes;
  EXPECT_EQ(bytes.begin(), bytes.end());
  EXPECT_EQ(bytes.cbegin(), bytes.cend());
  EXPECT_EQ(const_bytes.begin(), const_bytes.end());
}

TEST(ByteArrayTest, Hash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      ByteArray(),
      ByteArray("12345"),
      ByteArray("ABCDE"),
      ByteArray("A1B2Z"),
  }));
}

}  // namespace
