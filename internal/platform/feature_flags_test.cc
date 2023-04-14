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

#include "internal/platform/feature_flags.h"

#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace {

constexpr FeatureFlags::Flags kTestFeatureFlags{
    .enable_cancellation_flag = false,
    .keep_alive_interval_millis = 5000,
    .keep_alive_timeout_millis = 30000};

TEST(FeatureFlagsTest, ToSetFeatureWorks) {
  const FeatureFlags& features = FeatureFlags::GetInstance();
  EXPECT_TRUE(features.GetFlags().enable_cancellation_flag);

  EXPECT_EQ(5000, features.GetFlags().keep_alive_interval_millis);
  EXPECT_EQ(30000, features.GetFlags().keep_alive_timeout_millis);

  MediumEnvironment& medium_environment = MediumEnvironment::Instance();
  medium_environment.SetFeatureFlags(kTestFeatureFlags);
  EXPECT_FALSE(features.GetFlags().enable_cancellation_flag);

  const FeatureFlags& another_features_ref = FeatureFlags::GetInstance();
  EXPECT_FALSE(another_features_ref.GetFlags().enable_cancellation_flag);
}

}  // namespace
}  // namespace nearby
