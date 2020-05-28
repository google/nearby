#include "platform_v2/base/base64_utils.h"

#include "platform_v2/base/byte_array.h"
#include "absl/strings/escaping.h"

namespace location {
namespace nearby {

std::string Base64Utils::Encode(const ByteArray& bytes) {
  std::string base64_string;

  absl::WebSafeBase64Escape(std::string(bytes), &base64_string);

  return base64_string;
}

ByteArray Base64Utils::Decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return ByteArray();
  }

  return ByteArray(decoded_string.data(), decoded_string.size());
}

}  // namespace nearby
}  // namespace location
