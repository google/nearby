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
