#ifndef PLATFORM_BASE64_UTILS_H_
#define PLATFORM_BASE64_UTILS_H_

#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

class Base64Utils {
 public:
  static std::string encode(absl::string_view input);
  static std::string encode(const ByteArray& bytes);
  static std::string encode(ConstPtr<ByteArray> bytes);

  template <typename T>
  static T decode(absl::string_view base64_string);
  template <>
  Ptr<ByteArray> decode(absl::string_view base64_string);
  template <>
  ByteArray decode(absl::string_view base64_string);
  static Ptr<ByteArray> decode(absl::string_view base64_string) {
    return decode<Ptr<ByteArray>>(base64_string);
  }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE64_UTILS_H_
