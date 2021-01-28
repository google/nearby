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

#include "platform/base/feature_flags.h"

#include "platform/base/medium_environment.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

FeatureFlags::Flags kTestFeatureFlags{.enable_cancellation_flags = true};

TEST(FeatureFlagsTest, ToStringWorks) {
  const FeatureFlags& features = FeatureFlags::GetInstance();
  EXPECT_FALSE(features.GetFlags().enable_cancellation_flags);

  MediumEnvironment& medium_environment = MediumEnvironment::Instance();
  medium_environment.SetFeatureFlags(kTestFeatureFlags);
  EXPECT_TRUE(features.GetFlags().enable_cancellation_flags);

  const FeatureFlags& another_features_ref = FeatureFlags::GetInstance();
  EXPECT_TRUE(another_features_ref.GetFlags().enable_cancellation_flags);
}

}  // namespace
}  // namespace nearby
}  // namespace location
