#ifndef CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
#define CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_

#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class BLEPeripheral {
 public:
  explicit BLEPeripheral(ConstPtr<ByteArray> id);
  ~BLEPeripheral();

  ConstPtr<ByteArray> getId() const;

 private:
  // A unique identifier for this peripheral. It can be the BLE advertisement it
  // was found on, or even simply the BLE MAC address.
  ScopedPtr<ConstPtr<ByteArray>> id_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
