#include "platform_v2/public/atomic_boolean.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

TEST(AtomicBooleanTest, SetReturnsPrevoiusValue) {
  AtomicBoolean value(false);
  EXPECT_FALSE(value.Set(true));
  EXPECT_TRUE(value.Set(true));
}

TEST(AtomicBooleanTest, GetReturnsWhatWasSet) {
  AtomicBoolean value(false);
  EXPECT_FALSE(value.Set(true));
  EXPECT_TRUE(value.Get());
}

}  // namespace
}  // namespace nearby
}  // namespace location
