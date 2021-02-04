#include "platform/base/feature_flags.h"

#include "platform/base/medium_environment.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

constexpr FeatureFlags::Flags kTestFeatureFlags{.enable_cancellation_flag =
                                                    true};

TEST(FeatureFlagsTest, ToSetFeatureWorks) {
  const FeatureFlags& features = FeatureFlags::GetInstance();
  EXPECT_FALSE(features.GetFlags().enable_cancellation_flag);

  MediumEnvironment& medium_environment = MediumEnvironment::Instance();
  medium_environment.SetFeatureFlags(kTestFeatureFlags);
  EXPECT_TRUE(features.GetFlags().enable_cancellation_flag);

  const FeatureFlags& another_features_ref = FeatureFlags::GetInstance();
  EXPECT_TRUE(another_features_ref.GetFlags().enable_cancellation_flag);
}

}  // namespace
}  // namespace nearby
}  // namespace location
