#ifndef CORE_V2_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class BlePeripheral {
 public:
  BlePeripheral() = default;
  explicit BlePeripheral(const ByteArray& id) : id_(id) {}
  ~BlePeripheral() = default;

  BlePeripheral(const BlePeripheral&) = default;
  BlePeripheral& operator=(const BlePeripheral&) = default;
  BlePeripheral(BlePeripheral&&) = default;
  BlePeripheral& operator=(BlePeripheral&&) = default;

  bool IsValid() const { return !id_.Empty(); }
  ByteArray GetId() const { return id_; }

 private:
  // A unique identifier for this peripheral. It can be the BLE advertisement it
  // was found on, or even simply the BLE MAC address.
  ByteArray id_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
