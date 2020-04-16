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

#include "platform/prng.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

TEST(PrngTest, NextInt32) {
  std::int32_t i = Prng().nextInt32();
  ASSERT_LE(i, std::numeric_limits<std::int32_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::int32_t>::min());
}

TEST(PrngTest, NextUInt32) {
  std::uint32_t i = Prng().nextUInt32();
  ASSERT_LE(i, std::numeric_limits<std::uint32_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::uint32_t>::min());
}

TEST(PrngTest, NextInt64) {
  std::int64_t i = Prng().nextInt64();
  ASSERT_LE(i, std::numeric_limits<std::int64_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::int64_t>::min());
}

}  // namespace nearby
}  // namespace location
