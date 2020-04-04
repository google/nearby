#include "platform/base64_utils.h"

#include "absl/strings/escaping.h"

namespace location {
namespace nearby {

std::string Base64Utils::encode(ConstPtr<ByteArray> bytes) {
  std::string base64_string;

  if (!bytes.isNull()) {
    absl::WebSafeBase64Escape(bytes->asString(), &base64_string);
  }

  return base64_string;
}

std::string Base64Utils::encode(const ByteArray& bytes) {
  std::string base64_string;
  absl::WebSafeBase64Escape(bytes.asString(), &base64_string);

  return base64_string;
}

std::string Base64Utils::encode(absl::string_view input) {
  std::string base64_string;
  absl::WebSafeBase64Escape(input, &base64_string);

  return base64_string;
}

template<>
Ptr<ByteArray> Base64Utils::decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return Ptr<ByteArray>();
  }

  return MakePtr(new ByteArray(decoded_string));
}

template<>
ByteArray Base64Utils::decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return ByteArray();
  }

  return ByteArray(decoded_string);
}

Ptr<ByteArray> Base64Utils::decode(const std::string& base64_string) {
  return decode<Ptr<ByteArray>>(base64_string);
}

}  // namespace nearby
}  // namespace location
