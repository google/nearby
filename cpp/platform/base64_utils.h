#ifndef PLATFORM_BASE64_UTILS_H_
#define PLATFORM_BASE64_UTILS_H_

#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class Base64Utils {
 public:
  static std::string encode(const std::string& input);
  static std::string encode(const ByteArray& bytes);
  static std::string encode(ConstPtr<ByteArray> bytes);

  template <typename T>
  static T decode(const std::string& base64_string);
  template <>
  Ptr<ByteArray> decode(const std::string& base64_string);
  template <>
  ByteArray decode(const std::string& base64_string);
  static Ptr<ByteArray> decode(const std::string& base64_string) {
    return decode<Ptr<ByteArray>>(base64_string);
  }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE64_UTILS_H_
