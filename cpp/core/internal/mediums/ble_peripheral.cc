#include "core/internal/mediums/ble_peripheral.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

BLEPeripheral::BLEPeripheral(ConstPtr<ByteArray> id) : id_(id) {}

BLEPeripheral::~BLEPeripheral() {
  // Nothing to do.
}

ConstPtr<ByteArray> BLEPeripheral::getId() const { return id_.get(); }

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
