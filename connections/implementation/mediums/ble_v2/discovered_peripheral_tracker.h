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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_

#include <functional>
#include <memory>
#include <string>

#include "connections/implementation/mediums//lost_entity_tracker.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/lost_entity_tracker.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex.h"

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
        BleV2Peripheral& peripheral, int num_slots,
        AdvertisementReadResult* advertisement_read_result, int psm,
        const std::vector<std::string>& interesting_service_ids)>
        fetch_advertisements = [](BleV2Peripheral&, int,
                                  AdvertisementReadResult*, int,
                                  const std::vector<std::string>&)
        -> std::unique_ptr<AdvertisementReadResult> { return nullptr; };
  };

  // Starts tracking discoveries for a particular service Id.
  //
  // service_id                      - The service ID to track.
  // discovered_peripheral_callback  - The callback to invoke for discovery
  //                                   events.
  // fast_advertisement_service_uuid - The service UUID to look for fast
  //                                   advertisements on.
  // Note: fast_advertisement_service_uuid can be empty string to indicate
  // that `fast_advertisement_service_uuid` will be ignored for regular
  // advertisement.
  void StartTracking(
      const std::string& service_id,
      DiscoveredPeripheralCallback discovered_peripheral_callback,
      const std::string& fast_advertisement_service_uuid)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops tracking discoveries for a particular service Id.
  void StopTracking(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Processes a found BLE advertisement.
  //
  // peripheral         - To access the status of the operation. The ownership
  //                      is moved into 'discovered_peripheral_tracker' object.
  // advertisement_data - The found BLE advertisement data.
  // advertisement_fetcher - The advertisement fetcher callback for regular
  //                         advertisement if the advertisement header is been
  //                         processed.
  void ProcessFoundBleAdvertisement(
      BleV2Peripheral peripheral,
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      AdvertisementFetcher advertisement_fetcher) ABSL_LOCKS_EXCLUDED(mutex_);

  // Processes the set of lost GATT advertisements and notifies the client of
  // any lost peripherals.
  void ProcessLostGattAdvertisements() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  using BleAdvertisementSet = absl::flat_hash_set<BleAdvertisement>;

  struct ServiceIdInfo {
    // Tracks what service IDs are currently active and gives us client
    // callbacks to call.
    DiscoveredPeripheralCallback discovered_peripheral_callback;

    // Used to periodically compute lost GATT advertisements.
    std::unique_ptr<LostEntityTracker<BleAdvertisement>> lost_entity_tracker;

    // Used to check for fast advertisements delivered through BLE advertisement
    // service data, under the given UUID.
    std::string fast_advertisement_service_uuid;
  };

  struct GattAdvertisementInfo {
    // GATT advertisement to the service ID it's associated with. Tracks what
    // GATT advertisements are currently active. Used to determine which
    // LostEntityTracker to invoke when advertisements are rediscovered.
    std::string service_id;

    // Used to efficiently find advertisement headers to delete when GATT
    // advertisements are updated. This is a reverse map of
    // gatt_advertisements_.
    BleAdvertisementHeader advertisement_header;

    // Used when we need to make a socket connection based off of the GATT
    // advertisement alone. Entries are modified every time a GATT
    // advertisement's advertisement header is seen.
    std::string mac_address;
  };

  void ClearDataForServiceId(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Clears out all data related to the provided GATT advertisement. This
  // includes:
  //   1. Removing the corresponding GATT advertisement from
  //      gatt_advertisement_infos_.
  //   2. Removing the corresponding advertisement header from
  //      advertisement_read_results.
  //   3. Removing the corresponding advertisement header from
  //      gatt_advertisements_, only if there are no remaining GATT
  //      advertisements related to that header.
  void ClearGattAdvertisement(const BleAdvertisement& gatt_advertisement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::vector<std::string> GetTrackedServiceIds()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Handles the legacy fast advertisement or the extended fast/regular
  // advertisement.
  void HandleAdvertisement(
      BleV2Peripheral& peripheral,
      const api::ble_v2::BleAdvertisementData& advertisement_data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ByteArray ExtractInterestedAdvertisementBytes(
      const api::ble_v2::BleAdvertisementData& advertisement_data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Creates an advertisement header that's purely a hash of the fast/regular
  // advertisement, since they come with no header.
  BleAdvertisementHeader CreateAdvertisementHeader(
      const ByteArray& advertisement_bytes);

  // Returns BleAdvertisementHeader, it may be replaced if the header is mock
  // and there's a psm value in advertisement.
  BleAdvertisementHeader HandleRawGattAdvertisements(
      const BleAdvertisementHeader& advertisement_header,
      const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
      const std::string& service_uuid) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns a map of service IDs to GATT advertisements who belong to a tracked
  // service ID.
  absl::flat_hash_map<std::string, BleAdvertisement> ParseRawGattAdvertisements(
      const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
      const std::string& service_uuid) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool ShouldNotifyForNewPsm(int old_psm, int new_psm) const {
    return new_psm != BleAdvertisementHeader::kDefaultPsmValue &&
           old_psm != new_psm;
  }
  bool ShouldRemoveHeader(
      const BleAdvertisementHeader& old_advertisement_header,
      const BleAdvertisementHeader& new_advertisement_header);
  bool IsMockedAdvertisementHeader(
      const BleAdvertisementHeader& advertisement_header);
  void UpdateCommonStateForFoundBleAdvertisement(
      const BleAdvertisementHeader& advertisement_header,
      const std::string& mac_address) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  BlePeripheral GenerateBlePeripheral(
      const BleAdvertisement& gatt_advertisement,
      int psm = BleAdvertisementHeader::kDefaultPsmValue);

  Mutex mutex_;

  // ------------ SERVICE ID MAPS ------------
  // Entries in these maps all follow the same lifecycle. Entries are added in
  // StartTracking, and removed in StopTracking.
  absl::flat_hash_map<std::string, ServiceIdInfo> service_id_infos_
      ABSL_GUARDED_BY(mutex_);

  // ------------ ADVERTISEMENT HEADER MAPS ------------
  // Maps advertisement headers to AdvertisementReadResult. Tells us when to
  // retry reading a GATT advertisement. If no entry exists for a particular
  // header, we should try reading a GATT advertisement. Entries are added
  // whenever a GATT advertisement read is attempted, and removed when GATT
  // advertisements are lost. Entries are also removed whenever
  // gattAdvertisements removes its entry.
  //
  // The map is also cleared whenever StartTracking is called, due to client
  // changes. For example, say clients A and B start scanning and discover
  // advertisements A and B (for both clients) on advertisement header 1. Then,
  // A restarts scanning, causing us to clear stale advertisement A. However,
  // since B was still scanning, we don't remove advertisement header 1 from the
  // map. This causes us to never re-read advertisement A.
  absl::flat_hash_map<BleAdvertisementHeader,
                      std::unique_ptr<AdvertisementReadResult>>
      advertisement_read_results_;

  // Maps advertisement headers to a set of GATT advertisements from a single
  // peripheral. Used to retrieve GATT advertisements that we need to reprocess
  // every time a header is seen. Entries are added when GATT advertisements are
  // read, removed when all associated GATT advertisements are lost or become
  // stale, and replaced when the advertisement header is updated for a single
  // remote peripheral.
  absl::flat_hash_map<BleAdvertisementHeader, BleAdvertisementSet>
      gatt_advertisements_;

  // ------------ GATT ADVERTISEMENT MAPS ------------
  // Entries are added when Gatt advertisements are read, and removed when Gatt
  // advertisements are lost or become stale.
  absl::flat_hash_map<BleAdvertisement, GattAdvertisementInfo>
      gatt_advertisement_infos_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_
