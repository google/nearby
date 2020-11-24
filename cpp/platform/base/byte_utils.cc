#include "platform/base/byte_utils.h"

#include <cstdlib>

#include "platform/base/base_input_stream.h"
#include "absl/strings/str_format.h"

namespace location {
namespace nearby {

std::string ByteUtils::ToFourDigitString(ByteArray& bytes) {
  int multiplier = 1;
  int hashCode = 0;

  BaseInputStream base_input_stream{bytes};
  while (base_input_stream.IsAvailable(1)) {
    auto byte = static_cast<int>(base_input_stream.ReadUint8());
    hashCode = (hashCode + byte * multiplier) % kHashBasePrime;
    multiplier = multiplier * kHashBaseMultiplier % kHashBasePrime;
  }
  return absl::StrFormat("%04d", abs(hashCode));
}

}  // namespace nearby
}  // namespace location
