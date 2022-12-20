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

#include "presence/discovery_options.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {

constexpr bool kTestLocalWifiOnly = false;
TEST(DiscoveryOptionsTest, NoDefaultConstructor) {
  EXPECT_FALSE(std::is_trivially_constructible<DiscoveryOptions>::value);
}

TEST(DiscoveryOptionsTest, ExplicitInitEquals) {
  DiscoveryOptions option1 = {kTestLocalWifiOnly};
  DiscoveryOptions option2 = {kTestLocalWifiOnly};
  EXPECT_EQ(option1, option2);
  EXPECT_EQ(option1.local_wifi_only_, kTestLocalWifiOnly);
}

TEST(DiscoveryOptionsTest, ExplicitInitNotEquals) {
  DiscoveryOptions option1 = {kTestLocalWifiOnly};
  DiscoveryOptions option2 = {!kTestLocalWifiOnly};
  EXPECT_NE(option1, option2);
}

TEST(DiscoveryOptionsTest, CopyInitEquals) {
  DiscoveryOptions option1 = {kTestLocalWifiOnly};
  DiscoveryOptions option2 = {option1};
  EXPECT_EQ(option1, option2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
