#include "platform/base/cancellation_flag.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

TEST(CancellationFlagTest, InitialValueIsFalse) {
  CancellationFlag flag;
  EXPECT_FALSE(flag.Cancelled());
}

TEST(CancellationFlagTest, CanCancel) {
  CancellationFlag flag;
  flag.Cancel();
  EXPECT_TRUE(flag.Cancelled());
}

}  // namespace nearby
}  // namespace location
