#ifndef CORE_V2_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_

#include "core_v2/internal/mediums/ble_v2/ble_peripheral.h"
#include "core_v2/listeners.h"
#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

/** Callback that is invoked when a {@link BlePeripheral} is discovered. */
struct DiscoveredPeripheralCallback {
  std::function<void(BlePeripheral& peripheral, const std::string& service_id,
                     const ByteArray& advertisement_byts,
                     bool fast_advertisement)>
      peripheral_discovered_cb =
          DefaultCallback<BlePeripheral&, const std::string&, const ByteArray&,
                          bool>();
  std::function<void(BlePeripheral& peripheral, const std::string& service_id)>
      peripheral_lost_cb =
          DefaultCallback<BlePeripheral&, const std::string&>();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_
