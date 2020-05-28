#include "platform_v2/base/prng.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

TEST(PrngTest, NextInt32) {
  std::int32_t i = Prng().NextInt32();
  EXPECT_LE(i, std::numeric_limits<std::int32_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::int32_t>::min());
}

TEST(PrngTest, NextUInt32) {
  std::uint32_t i = Prng().NextUint32();
  EXPECT_LE(i, std::numeric_limits<std::uint32_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::uint32_t>::min());
}

TEST(PrngTest, NextInt64) {
  std::int64_t i = Prng().NextInt64();
  EXPECT_LE(i, std::numeric_limits<std::int64_t>::max());
  EXPECT_GE(i, std::numeric_limits<std::int64_t>::min());
}

}  // namespace nearby
}  // namespace location
