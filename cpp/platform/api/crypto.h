#ifndef PLATFORM_API_CRYPTO_H_
#define PLATFORM_API_CRYPTO_H_

#include "platform/base/byte_array.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// A provider of standard hashing algorithms.
class Crypto {
 public:
  // Initialize global crypto state.
  static void Init();
  // Return MD5 hash of input.
  static ByteArray Md5(absl::string_view input);
  // Return SHA256 hash of input.
  static ByteArray Sha256(absl::string_view input);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_CRYPTO_H_
