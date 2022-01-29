// Copyright 2022 Google LLC
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

#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

using ::nearby::cal::BleAdvertisementData;

// TODO(edwinwu): Implementation.
void DiscoveredPeripheralTracker::StartTracking(
    const std::string& service_id,
    DiscoveredPeripheralCallback discovered_peripheral_callback,
    const std::string& fast_advertisement_service_uuid) {}

void DiscoveredPeripheralTracker::StopTracking(const std::string& service_id) {}

void DiscoveredPeripheralTracker::ProcessFoundBleAdvertisement(
    const BleAdvertisementData& advertisement_data,
    AdvertisementFetcher advertisement_fetcher) {
}

void DiscoveredPeripheralTracker::ProcessLostGattAdvertisements() {}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
