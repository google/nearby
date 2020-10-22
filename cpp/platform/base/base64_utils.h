#ifndef PLATFORM_BASE_BASE64_UTILS_H_
#define PLATFORM_BASE_BASE64_UTILS_H_

#include "platform/base/byte_array.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

class Base64Utils {
 public:
  static std::string Encode(const ByteArray& bytes);
  static ByteArray Decode(absl::string_view base64_string);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_BASE64_UTILS_H_
