#ifndef CORE_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_UTILS_H_

#include "platform/api/bluetooth_classic.h"
#include "platform/api/hash_utils.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

class Utils {
 public:
  static void closeSocket(Ptr<BluetoothServerSocket> socket,
                          const std::string& type, const std::string& name);
  static ConstPtr<ByteArray> sha256Hash(Ptr<HashUtils> hash_utils,
                                        ConstPtr<ByteArray> source,
                                        size_t length);
  static ConstPtr<ByteArray> legacySha256HashOnlyForPrinting(
      Ptr<HashUtils> hash_utils, ConstPtr<ByteArray> source, size_t length);

  static ConstPtr<ByteArray> generateRandomBytes(size_t length);

 private:
  static std::string bytesToPrintableHexString(ConstPtr<ByteArray> bytes);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_UTILS_H_
