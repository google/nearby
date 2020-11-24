#ifndef PLATFORM_BASE_BYTE_UTILS_H_
#define PLATFORM_BASE_BYTE_UTILS_H_

#include "platform/base/byte_array.h"

namespace location {
namespace nearby {

class ByteUtils {
 public:
  static std::string ToFourDigitString(ByteArray& bytes);

 private:
  // The biggest prime number under 10000, used as a mod base to trim integers
  // into 4 digits.
  static constexpr int kHashBasePrime = 9973;
  // The hash multiplier.
  static constexpr int kHashBaseMultiplier = 31;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_BYTE_UTILS_H_
