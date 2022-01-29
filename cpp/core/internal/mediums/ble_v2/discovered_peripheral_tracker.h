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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_

#include <memory>

#include "third_party/nearby/cpp/cal/base/ble_types.h"
#include "third_party/nearby/cpp/core/internal/mediums/ble_v2/advertisement_read_result.h"
#include "third_party/nearby/cpp/core/internal/mediums/ble_v2/discovered_peripheral_callback.h"
#include "third_party/nearby/cpp/platform/public/ble_v2.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Manages all discovered peripheral logic for {@link BluetoothLowEnergy}. This
// includes tracking found peripherals, lost peripherals, and MAC addresses
// associated with those peripherals.
//
// See go/ble-on-lost for more information. It includes the algorithms used to
// compute found and lost peripherals.
class DiscoveredPeripheralTracker {
 public:
  // GATT advertisement fetcher.
  struct AdvertisementFetcher {
    // Fetches relevant GATT advertisements for the peripheral found in {@link
    // DiscoveredPeripheralTracker#ProcessFoundBleAdvertisement(}.
    std::function<std::unique_ptr<AdvertisementReadResult>(
        BlePeripheralV2& peripheral, int num_slots,
        AdvertisementReadResult* advertisement_read_result)>
        fetch_advertisements =
            [](BlePeripheralV2&, int, AdvertisementReadResult*)
        -> std::unique_ptr<AdvertisementReadResult> { return nullptr; };
  };

  DiscoveredPeripheralTracker() = default;
  ~DiscoveredPeripheralTracker() = default;

  // Starts tracking discoveries for a particular service Id.
  void StartTracking(
      const std::string& service_id,
      DiscoveredPeripheralCallback discovered_peripheral_callback,
      const std::string& fast_advertisement_service_uuid);

  // Stops tracking discoveries for a particular service Id.
  void StopTracking(const std::string& service_id);

  // Processes a found BLE advertisement.
  void ProcessFoundBleAdvertisement(
      const ::nearby::cal::BleAdvertisementData& advertisement_data,
      AdvertisementFetcher advertisement_fetcher);

  // Processes the set of lost GATT advertisements and notifies the client of
  // any lost peripherals.
  void ProcessLostGattAdvertisements();
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_
