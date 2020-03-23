#ifndef CORE_INTERNAL_BLE_COMPAT_H_
#define CORE_INTERNAL_BLE_COMPAT_H_

#ifndef BLE_V2_IMPLEMENTED
// Flip to true when BLE_V2 is fully implemented and ready to be tested.
#define BLE_V2_IMPLEMENTED 0
#endif

#if BLE_V2_IMPLEMENTED

#include "core/internal/mediums/ble_peripheral.h"
#include "core/internal/mediums/discovered_peripheral_callback.h"
#define BLE_PERIPHERAL location::nearby::connections::mediums::BLEPeripheral
#define DISCOVERED_PERIPHERAL_CALLBACK \
  location::nearby::connections::mediums::DiscoveredPeripheralCallback

#else

#include "platform/api/ble.h"
#define BLE_PERIPHERAL location::nearby::BLEPeripheral
#define DISCOVERED_PERIPHERAL_CALLBACK \
  BLE<Platform>::DiscoveredPeripheralCallback

#endif  // BLE_V2_IMPLEMENTED

#endif  // CORE_INTERNAL_BLE_COMPAT_H_
