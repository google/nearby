#ifndef CORE_V2_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_V2_INTERNAL_MEDIUMS_UTILS_H_

#include <memory>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {

class Utils {
 public:
  static ByteArray GenerateRandomBytes(size_t length);
  static ByteArray Sha256Hash(const ByteArray& source, size_t length);
  static ByteArray Sha256Hash(const std::string& source, size_t length);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_UTILS_H_
