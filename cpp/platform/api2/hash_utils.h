#ifndef PLATFORM_API2_HASH_UTILS_H_
#define PLATFORM_API2_HASH_UTILS_H_

#include "platform/byte_array.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// A provider of standard hashing algorithms.
class HashUtils {
 public:
  static ByteArray Md5(absl::string_view input);
  static ByteArray Sha256(absl::string_view input);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_HASH_UTILS_H_
