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

#include "platform/byte_array.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using location::nearby::ByteArray;

TEST(ByteArrayTest, DefaultSizeIsZero) {
  ByteArray bytes;
  ASSERT_EQ(0, bytes.size());
}

TEST(ByteArrayTest, SetFromString) {
  std::string setup("setup_test");
  ByteArray bytes{setup};  // array initialized with a copy of string.
  ASSERT_EQ(setup.size(), bytes.size());
  ASSERT_EQ(bytes.asString(), setup);
}

TEST(ByteArrayTest, SetExplicitSize) {
  constexpr size_t kArraySize = 10;
  char reference[kArraySize]{};
  ByteArray bytes{kArraySize};  // array of size 10, zero-initialized.
  ASSERT_EQ(kArraySize, bytes.size());
  ASSERT_EQ(0, memcmp(bytes.getData(), reference, kArraySize));
}

TEST(ByteArrayTest, SetExplicitData) {
  constexpr static const char message[] {"test_message"};
  constexpr size_t kMessageSize = sizeof(message);
  ByteArray bytes{message, kMessageSize};
  ASSERT_EQ(kMessageSize, bytes.size());
  ASSERT_NE(message, bytes.getData());
  ASSERT_EQ(0, memcmp(message, bytes.getData(), kMessageSize));
}

}  // namespace
