#include "platform/prng.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

TEST(PrngTest, NextInt32) {
  std::int32_t i = Prng().nextInt32();
  ASSERT_LE(i, std::numeric_limits<std::int32_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::int32_t>::min());
}

TEST(PrngTest, NextUInt32) {
  std::uint32_t i = Prng().nextUInt32();
  ASSERT_LE(i, std::numeric_limits<std::uint32_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::uint32_t>::min());
}

TEST(PrngTest, NextInt64) {
  std::int64_t i = Prng().nextInt64();
  ASSERT_LE(i, std::numeric_limits<std::int64_t>::max());
  ASSERT_GE(i, std::numeric_limits<std::int64_t>::min());
}

}  // namespace nearby
}  // namespace location
