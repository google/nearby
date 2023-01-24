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
    //
    // `advertisement_read_result` is in/out mutable reference that the caller
    // should take of its life cycle and pass a valid reference.
    std::function<void(
        BleV2Peripheral peripheral, int num_slots, int psm,
        const std::vector<std::string>& interesting_service_ids,
        mediums::AdvertisementReadResult& advertisement_read_result)>
        fetch_advertisements = [](BleV2Peripheral, int, int,
                                  const std::vector<std::string>&,
                                  mediums::AdvertisementReadResult&) {};
  };

  explicit DiscoveredPeripheralTracker(
      bool is_extended_advertisement_available = false)
      : is_extended_advertisement_available_(
            is_extended_advertisement_available) {}

  // Starts tracking discoveries for a particular service Id.
  //
  // service_id                      - The service ID to track.
  // discovered_peripheral_callback  - The callback to invoke for discovery
  //                                   events.
  // fast_advertisement_service_uuid - The service UUID to look for fast
  //                                   advertisements on.
  // Note: fast_advertisement_service_uuid can be empty UUID to indicate
  // that `fast_advertisement_service_uuid` will be ignored for regular
  // advertisement.
  void StartTracking(
      const std::string& service_id,
      const DiscoveredPeripheralCallback& discovered_peripheral_callback,
      const Uuid& fast_advertisement_service_uuid) ABSL_LOCKS_EXCLUDED(mutex_);

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
      api::ble_v2::BleAdvertisementData advertisement_data,
      AdvertisementFetcher advertisement_fetcher) ABSL_LOCKS_EXCLUDED(mutex_);

  // Processes the set of lost GATT advertisements and notifies the client of
  // any lost peripherals.
  void ProcessLostGattAdvertisements() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  using BleAdvertisementSet = absl::flat_hash_set<BleAdvertisement>;

  // A container to hold callback or other informations that bring from BLE
  // medium when `StartTracking`.
  struct ServiceIdInfo {
    // Tracks what service IDs are currently active and gives us client
    // callbacks to call.
    DiscoveredPeripheralCallback discovered_peripheral_callback;

    // Used to periodically compute lost GATT advertisements.
    std::unique_ptr<LostEntityTracker<BleAdvertisement>> lost_entity_tracker;

    // Used to check for fast advertisements delivered through BLE advertisement
    // service data, under the given UUID.
    Uuid fast_advertisement_service_uuid;
  };

  // A container to hold the related informations for a GATT advertisement.
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

    // A proxy BlePeripheral for found/lost disovery callback.
    BleV2Peripheral peripheral;
  };

  // Clears stale data from any previous sessions.
  void ClearDataForServiceId(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if `advertisement_data` is AdvertisementHeader and is marked
  // as exented_advertisement. This is to avoid reading advertisement from GATT
  // connection, which has been advertised by extended advertisement.
  bool IsSkippableGattAdvertisement(
      const api::ble_v2::BleAdvertisementData& advertisement_data)
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

  // Handles the legacy fast advertisement or the extended fast/regular
  // advertisement.
  void HandleAdvertisement(
      BleV2Peripheral peripheral,
      const api::ble_v2::BleAdvertisementData& advertisement_data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Extracts the advertisement byte array from `AdvertisementData`.
  ByteArray ExtractInterestingAdvertisementBytes(
      const api::ble_v2::BleAdvertisementData& advertisement_data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Creates an advertisement header that's purely a hash of the fast/regular
  // advertisement, since they come with no header.
  BleAdvertisementHeader CreateAdvertisementHeader(
      const ByteArray& advertisement_bytes);

  // Returns BleAdvertisementHeader, it may be replaced if the header is mock
  // and there's a psm value in advertisement.
  BleAdvertisementHeader HandleRawGattAdvertisements(
      BleV2Peripheral peripheral,
      const BleAdvertisementHeader& advertisement_header,
      const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
      const Uuid& service_uuid) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns a map of service IDs to GATT advertisements who belong to a tracked
  // service ID.
  absl::flat_hash_map<std::string, BleAdvertisement> ParseRawGattAdvertisements(
      const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
      const Uuid& service_uuid) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if `new_psm` is not default value and different with
  // `old_psm`.
  bool ShouldNotifyForNewPsm(int old_psm, int new_psm) const;

  // Returns true if two headers are not the same.
  bool ShouldRemoveHeader(
      const BleAdvertisementHeader& old_advertisement_header,
      const BleAdvertisementHeader& new_advertisement_header);

  // Returns true if it is a faked advertisement header which is purely a hash
  // of the fast/regular advertisement, since they come with no header.
  bool IsDummyAdvertisementHeader(
      const BleAdvertisementHeader& advertisement_header);

  // Handles the advertisement header for regular advertisement.
  void HandleAdvertisementHeader(
      BleV2Peripheral peripheral,
      const api::ble_v2::BleAdvertisementData& advertisement_data,
      AdvertisementFetcher advertisement_fetcher)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Extracts the advertisement header byte array from `AdvertisementData`.
  ByteArray ExtractAdvertisementHeaderBytes(
      const api::ble_v2::BleAdvertisementData& advertisement_data)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if the advertisement header contains a service ID we're
  // tracking.
  bool IsInterestingAdvertisementHeader(
      const BleAdvertisementHeader& advertisement_header)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if the `advertisement_header` is allowd to callbck to fecth
  // advertisement.
  bool ShouldReadRawAdvertisementFromServer(
      const BleAdvertisementHeader& advertisement_header)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Fetches advertsiement from BLE medium if advertisement header is read in
  // AdvertisementData.
  //
  // advertisement_fetcher : a fetcher passed from BLE medium to read the
  // advertisemeent from BLE characteristics by GATT server.
  std::vector<const ByteArray*> FetchRawAdvertisements(
      BleV2Peripheral peripheral,
      const BleAdvertisementHeader& advertisement_header,
      AdvertisementFetcher advertisement_fetcher)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Updates `gatt_advertisement_infos_` map no matter whether we read a new
  // GATT advertisement by the input `advertisement_header` and 'mac_address`.
  void UpdateCommonStateForFoundBleAdvertisement(
      const BleAdvertisementHeader& advertisement_header)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Mutex mutex_;
  bool is_extended_advertisement_available_;

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
      advertisement_read_results_ ABSL_GUARDED_BY(mutex_);

  // Maps advertisement headers to a set of GATT advertisements from a single
  // peripheral. Used to retrieve GATT advertisements that we need to reprocess
  // every time a header is seen. Entries are added when GATT advertisements are
  // read, removed when all associated GATT advertisements are lost or become
  // stale, and replaced when the advertisement header is updated for a single
  // remote peripheral.
  absl::flat_hash_map<BleAdvertisementHeader, BleAdvertisementSet>
      gatt_advertisements_ ABSL_GUARDED_BY(mutex_);

  // ------------ GATT ADVERTISEMENT MAPS ------------
  // Entries are added when Gatt advertisements are read, and removed when Gatt
  // advertisements are lost or become stale.
  absl::flat_hash_map<BleAdvertisement, GattAdvertisementInfo>
      gatt_advertisement_infos_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_DISCOVERED_PERIPHERAL_TRACKER_H_
