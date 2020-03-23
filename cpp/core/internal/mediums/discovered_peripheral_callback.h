#ifndef CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_CALLBACK_H_
#define CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_CALLBACK_H_

#include "core/internal/mediums/ble_peripheral.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

/** Callback that is invoked when a {@link BLEPeripheral} is discovered. */
class DiscoveredPeripheralCallback {
 public:
  virtual ~DiscoveredPeripheralCallback() {}

  virtual void onPeripheralDiscovered(Ptr<BLEPeripheral> ble_peripheral,
                                      const string& service_id,
                                      ConstPtr<ByteArray> advertisement,
                                      bool is_fast_advertisement) = 0;
  virtual void onPeripheralLost(Ptr<BLEPeripheral> ble_peripheral,
                                const string& service_id);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_CALLBACK_H_
