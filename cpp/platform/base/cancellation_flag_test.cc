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

#include "platform/base/cancellation_flag.h"

#include "platform/base/feature_flags.h"
#include "platform/base/medium_environment.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

using FeatureFlags = FeatureFlags::Flags;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

class CancellationFlagTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  void SetUp() override {
    feature_flags_ = GetParam();
    env_.SetFeatureFlags(feature_flags_);
  }

  FeatureFlags feature_flags_;
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(CancellationFlagTest, InitialValueIsFalse) {
  CancellationFlag flag;

  // No matter FeatureFlag is enabled or not, Cancelled is always false.
  EXPECT_FALSE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, InitialValueAsTrue) {
  CancellationFlag flag{true};

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags_.enable_cancellation_flag) {
    EXPECT_FALSE(flag.Cancelled());
    return;
  }

  EXPECT_TRUE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, CanCancel) {
  CancellationFlag flag;
  flag.Cancel();

  // If FeatureFlag is disabled, return as no-op immediately and
  // Cancelled is always false.
  if (!feature_flags_.enable_cancellation_flag) {
    EXPECT_FALSE(flag.Cancelled());
    return;
  }

  EXPECT_TRUE(flag.Cancelled());
}

INSTANTIATE_TEST_SUITE_P(ParametrisedCancellationFlagTest, CancellationFlagTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace nearby
}  // namespace location
