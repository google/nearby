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

#include "presence/broadcast_options.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {

constexpr std::int64_t kReportingIntervalMillis1 = 1000;
constexpr std::int64_t kReportingIntervalMillis2 = 2000;
TEST(BroadcastOptionsTest, NoDefaultConstructor) {
  EXPECT_FALSE(std::is_trivially_constructible<BroadcastOptions>::value);
}

TEST(BroadcastOptionsTest, ExplicitInitEquals) {
  BroadcastOptions option1 = {kReportingIntervalMillis1};
  BroadcastOptions option2 = {kReportingIntervalMillis1};
  EXPECT_EQ(option1, option2);
  EXPECT_EQ(option1.reporting_interval_millis, kReportingIntervalMillis1);
}

TEST(BroadcastOptionsTest, ExplicitInitNotEquals) {
  BroadcastOptions option1 = {kReportingIntervalMillis1};
  BroadcastOptions option2 = {kReportingIntervalMillis2};
  EXPECT_NE(option1, option2);
}

TEST(BroadcastOptionsTest, CopyInitEquals) {
  BroadcastOptions option1 = {kReportingIntervalMillis1};
  BroadcastOptions option2 = {option1};

  EXPECT_EQ(option1, option2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
