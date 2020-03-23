#ifndef PLATFORM_API_HASH_UTILS_H_
#define PLATFORM_API_HASH_UTILS_H_

#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// A provider of standard hashing algorithms.
class HashUtils {
 public:
  virtual ~HashUtils() {}

  virtual ConstPtr<ByteArray> md5(const std::string& input) = 0;
  virtual ConstPtr<ByteArray> sha256(const std::string& input) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_HASH_UTILS_H_
