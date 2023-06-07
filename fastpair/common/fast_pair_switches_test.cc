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

#include "fastpair/common/fast_pair_switches.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace switches {
namespace {

TEST(FastPairSwitches, DefautEmptyHttpHost) {
  EXPECT_THAT(GetNearbyFastPairHttpHost(), testing::IsEmpty());
}

TEST(FastPairSwitches, SetLongSizedHttpHostSkipped) {
  std::string s(5000, '*');
  SetNearbyFastPairHttpHost(s);
  EXPECT_THAT(GetNearbyFastPairHttpHost(), testing::IsEmpty());
}

TEST(FastPairSwitches, SetNonEmptyHttpHost) {
  SetNearbyFastPairHttpHost("fakehost.com");
  EXPECT_THAT(GetNearbyFastPairHttpHost(), "fakehost.com");
}

TEST(FastPairSwitches, SetEmptyHttpHost) {
  SetNearbyFastPairHttpHost("");
  EXPECT_THAT(GetNearbyFastPairHttpHost(), testing::IsEmpty());
}

}  // namespace
}  // namespace switches
}  // namespace fastpair
}  // namespace nearby
