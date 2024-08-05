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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

// Callback that is invoked when a {@link BlePeripheral} is discovered.
struct DiscoveredPeripheralCallback {
  absl::AnyInvocable<void(
      BleV2Peripheral peripheral, const std::string& service_id,
      const ByteArray& advertisement_bytes, bool fast_advertisement)>
      peripheral_discovered_cb =
          [](BleV2Peripheral, const std::string&, const ByteArray&, bool) {};
  absl::AnyInvocable<void(
      BleV2Peripheral peripheral, const std::string& service_id,
      const ByteArray& advertisement_bytes, bool fast_advertisement)>
      peripheral_lost_cb =
          [](BleV2Peripheral, const std::string&, const ByteArray&, bool) {};
  absl::AnyInvocable<void(
      BleV2Peripheral peripheral, const std::string& service_id,
      const ByteArray& advertisement_bytes, bool fast_advertisement)>
      instant_lost_cb =
          [](BleV2Peripheral, const std::string&, const ByteArray&, bool) {};
  absl::AnyInvocable<void(void)> legacy_device_discovered_cb = []() {};
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_CALLBACK_H_
