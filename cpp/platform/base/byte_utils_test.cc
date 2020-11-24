#include "platform/base/byte_utils.h"

#include "platform/base/byte_array.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {

constexpr absl::string_view kFooBytes{"rawABCDE"};
constexpr absl::string_view kFooFourDigitsToken{"0392"};
constexpr absl::string_view kEmptyFourDigitsToken{"0000"};

TEST(ByteUtilsTest, ToFourDigitStringCorrect) {
  ByteArray bytes{std::string(kFooBytes)};

  auto four_digit_string = ByteUtils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kFooFourDigitsToken), four_digit_string);
}

TEST(ByteUtilsTest, TestEmptyByteArrayCorrect) {
  ByteArray bytes;

  auto four_digit_string = ByteUtils::ToFourDigitString(bytes);

  EXPECT_EQ(std::string(kEmptyFourDigitsToken), four_digit_string);
}

}  // namespace nearby
}  // namespace location
