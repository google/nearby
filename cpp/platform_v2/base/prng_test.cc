#include "platform_v2/base/prng.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {

enum class TestMode {
  kUpperHalfOfInt64,
  kLowerHalfOfInt64,
  kInt32,
  kUint32,
};

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

void ValidateRandom(TestMode mode) {
  int count_all_zeros = 0;
  int count_all_ones = 0;
  std::uint32_t i;
  Prng prng;
  for (int count = 0; count < 100; ++count) {
    switch (mode) {
      case TestMode::kUpperHalfOfInt64:
        i = static_cast<std::uint32_t>(prng.NextInt64() >> 32);
        break;
      case TestMode::kLowerHalfOfInt64:
        i = static_cast<std::uint32_t>(prng.NextInt64());
        break;
      case TestMode::kInt32:
        i = static_cast<std::uint32_t>(prng.NextInt32());
        break;
      case TestMode::kUint32:
        i = static_cast<std::uint32_t>(prng.NextUint32());
        break;
    }
    if (!i) count_all_zeros++;
    if (i == 0xFFFFFFFF) count_all_ones++;
  }
  EXPECT_LE(count_all_zeros, 1);
  EXPECT_LE(count_all_ones, 1);
}

TEST(PrngTest, ValidateUpperHalfOfInt64) {
  ValidateRandom(TestMode::kUpperHalfOfInt64);
}

TEST(PrngTest, ValidateLowerHalfOfInt64) {
  ValidateRandom(TestMode::kLowerHalfOfInt64);
}

TEST(PrngTest, ValidateInt32) {
  ValidateRandom(TestMode::kInt32);
}

TEST(PrngTest, ValidateUint32) {
  ValidateRandom(TestMode::kUint32);
}

}  // namespace nearby
}  // namespace location
