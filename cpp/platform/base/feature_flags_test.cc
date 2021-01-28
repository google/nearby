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
