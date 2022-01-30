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

#include <string>
#include <utility>

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

void DiscoveredPeripheralTracker::StartTracking(
    const std::string& service_id,
    DiscoveredPeripheralCallback discovered_peripheral_callback,
    const std::string& fast_advertisement_service_uuid) {
  MutexLock lock(&mutex_);

  ServiceIdInfo service_id_info = {
      .discovered_peripheral_callback =
          std::move(discovered_peripheral_callback),
      .lost_entity_tracker =
          absl::make_unique<LostEntityTracker<BleAdvertisement>>(),
      .fast_advertisement_service_uuid = fast_advertisement_service_uuid};

  // Replace if key is existed.
  service_id_infos_.insert_or_assign(service_id, std::move(service_id_info));

  // Clear all of the GATT read results. With this cleared, we will now attempt
  // to reconnect to every peripheral we see, giving us a chance to search for
  // the new service we're now tracking.
  // See the documentation of advertisementReadResult for more information.
  advertisement_read_results_.clear();

  // Remove stale data from any previous sessions.
  ClearDataForServiceId(service_id);
}

void DiscoveredPeripheralTracker::StopTracking(const std::string& service_id) {
  MutexLock lock(&mutex_);

  service_id_infos_.erase(service_id);
}

void DiscoveredPeripheralTracker::ProcessFoundBleAdvertisement(
    BleV2Peripheral peripheral,
    const ::location::nearby::api::ble_v2::BleAdvertisementData&
        advertisement_data,
    AdvertisementFetcher advertisement_fetcher) {}

void DiscoveredPeripheralTracker::ProcessLostGattAdvertisements() {}

void DiscoveredPeripheralTracker::ClearDataForServiceId(
    const std::string& service_id) {
  for (const auto& it : gatt_advertisement_infos_) {
    const GattAdvertisementInfo gatt_advertisement_info = it.second;
    if (gatt_advertisement_info.service_id != service_id) {
      continue;
    }
    ClearGattAdvertisement(it.first);
  }
}

void DiscoveredPeripheralTracker::ClearGattAdvertisement(
    const BleAdvertisement& gatt_advertisement) {
  const auto& gai_it = gatt_advertisement_infos_.find(gatt_advertisement);
  if (gai_it == gatt_advertisement_infos_.end()) {
    return;
  }
  auto item = gatt_advertisement_infos_.extract(gai_it);
  GattAdvertisementInfo& gatt_advertisement_info = item.mapped();

  const auto& ga_it =
      gatt_advertisements_.find(gatt_advertisement_info.advertisement_header);
  if (ga_it != gatt_advertisements_.end()) {
    // Remove the GATT advertisement from the advertisement header it's
    // associated with.
    BleAdvertisementSet& gatt_advertisement_set = ga_it->second;
    gatt_advertisement_set.erase(gatt_advertisement);

    // Unconditionally remove the header from advertisement_read_results_ so we
    // can attempt to reread the GATT advertisement if they return.
    advertisement_read_results_.erase(
        gatt_advertisement_info.advertisement_header);

    // If there are no more tracked GATT advertisements under this header, go
    // ahead and remove it from gatt_advertisements_.
    if (gatt_advertisement_set.empty()) {
      gatt_advertisements_.erase(ga_it);
    } else {  // or update the gatt_advertisement_set.
      gatt_advertisements_[gatt_advertisement_info.advertisement_header] =
          gatt_advertisement_set;
    }
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
