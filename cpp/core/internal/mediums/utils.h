#ifndef CORE_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_UTILS_H_

#include <memory>

#include "proto/connections/offline_wire_formats.pb.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {

class Utils {
 public:
  static ByteArray GenerateRandomBytes(size_t length);
  static ByteArray Sha256Hash(const ByteArray& source, size_t length);
  static ByteArray Sha256Hash(const std::string& source, size_t length);
  static std::string WrapUpgradeServiceId(const std::string& service_id);
  static std::string UnwrapUpgradeServiceId(const std::string& service_id);
  static LocationHint BuildLocationHint(const std::string& location);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_UTILS_H_
