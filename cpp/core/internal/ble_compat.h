// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
