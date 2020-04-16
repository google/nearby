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

#ifndef CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_TRACKER_H_
#define CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_TRACKER_H_

#include <cstdint>
#include <map>
#include <set>

#include "core/internal/mediums/advertisement_read_result.h"
#include "core/internal/mediums/ble_advertisement.h"
#include "core/internal/mediums/ble_advertisement_header.h"
#include "core/internal/mediums/ble_peripheral.h"
#include "core/internal/mediums/discovered_peripheral_callback.h"
#include "core/internal/mediums/lost_entity_tracker.h"
#include "platform/api/ble_v2.h"
#include "platform/api/hash_utils.h"
#include "platform/api/lock.h"
#include "platform/api/system_clock.h"
#include "platform/api/thread_utils.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

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
template <typename Platform>
class DiscoveredPeripheralTracker {
 public:
  DiscoveredPeripheralTracker();
  ~DiscoveredPeripheralTracker();

  void startTracking(
      const string& service_id,
      Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback,
      const string& fast_advertisement_service_uuid);
  void stopTracking(const string& service_id);

  // GATT advertisement fetcher.
  class GattAdvertisementFetcher {
   public:
    virtual ~GattAdvertisementFetcher() {}

    // Fetches relevant GATT advertisements for the peripheral found in {@link
    // DiscoveredPeripheralTracker#processFoundBleAdvertisement(BleSighting,
    // GattAdvertisementFetcher)}.
    virtual Ptr<AdvertisementReadResult<Platform>> fetchGattAdvertisements(
        Ptr<BLEPeripheralV2> ble_peripheral, std::int32_t num_slots,
        Ptr<AdvertisementReadResult<Platform>> advertisement_read_result) = 0;
  };
  void processFoundBleAdvertisement(
      Ptr<BLEPeripheralV2> ble_peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data,
      Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher);
  void processLostGattAdvertisements();

  // TODO(ahlee): Add connecting logic.

 private:
  static Ptr<BLEPeripheral> generateBlePeripheral(
      ConstPtr<BLEAdvertisement> gatt_advertisement);

  static const std::int32_t kMaxSlots;
  static const std::int64_t kMinConnectionDelayMillis;
  static const char* kCopresenceServiceUuid;

  std::set<string> getTrackedServiceIds();
  void clearDataForServiceId(const string& service_id);
  void clearGattAdvertisement(ConstPtr<BLEAdvertisement> gatt_advertisement);
  void handleFastAdvertisement(
      Ptr<BLEPeripheralV2> ble_peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data);
  void handleAdvertisementHeader(
      Ptr<BLEPeripheralV2> ble_peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data,
      Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher);
  string extractAdvertisementHeaderBytes(
      Ptr<BLEPeripheralV2> ble_peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data);
  ConstPtr<ByteArray> extractFastAdvertisementBytes(
      ConstPtr<BLEAdvertisementData> advertisement_data);
  /*RefCounted */ ConstPtr<BLEAdvertisementHeader>
  createFastAdvertisementHeader(ConstPtr<ByteArray> fast_advertisement_bytes);
  string createDummyAdvertisementHeaderBytes(
      Ptr<BLEPeripheralV2> ble_peripheral);
  bool isInterestingAdvertisementHeader(
      /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header);
  bool shouldReadFromAdvertisementGattServer(
      /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header);
  std::set<ConstPtr<ByteArray>> fetchRawGattAdvertisements(
      Ptr<BLEPeripheralV2> ble_peripheral,
      /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
      Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher);
  void handleRawGattAdvertisements(
      /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
      const std::set<ConstPtr<ByteArray>>& raw_gatt_advertisements,
      bool are_fast_advertisements);
  std::map<string, ConstPtr<BLEAdvertisement>> parseRawGattAdvertisements(
      const std::set<ConstPtr<ByteArray>>& raw_gatt_advertisements);
  void updateCommonStateForFoundBleAdvertisement(
      /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
      const string& mac_address);

  // TODO(ahlee): Add in connecting logic.

  // TODO(ahlee): Move these out to utils (also used by BLE V2).
  ConstPtr<ByteArray> generateAdvertisementHash(
      ConstPtr<ByteArray> advertisement_bytes);
  ConstPtr<ByteArray> generateServiceIdHash(
      BLEAdvertisement::Version::Value version, const string& service_id);

  // ------------ GENERAL ------------
  ScopedPtr<Ptr<Lock>> lock_;
  ScopedPtr<Ptr<ThreadUtils>> thread_utils_;
  ScopedPtr<Ptr<SystemClock>> system_clock_;
  ScopedPtr<Ptr<HashUtils>> hash_utils_;

  // ------------ SERVICE ID MAPS ------------
  // Entries in these maps all follow the same lifecycle. Entries are added in
  // startTracking, and removed in stopTracking.

  // Maps service IDs to DiscoveredPeripheralCallbacks. Tracks what service IDs
  // are currently active and gives us client callbacks to call.
  typedef std::map<string, Ptr<DiscoveredPeripheralCallback>>
      DiscoveredPeripheralCallbackMap;
  DiscoveredPeripheralCallbackMap discovered_peripheral_callbacks_;

  // Maps service IDs to LostEntityTrackers. Used to periodically compute lost
  // GATT advertisements, grouped by service ID.
  typedef std::map<string, Ptr<LostEntityTracker<Platform, BLEAdvertisement>>>
      LostEntityTrackerMap;
  LostEntityTrackerMap lost_entity_trackers_;

  // Maps service IDs to BLE service UUIDs. Used to check for fast
  // advertisements delivered through BLE advertisement service data, under the
  // given UUID.
  // UUIDs are represented as strings in this map because they are coming from
  // AdvertisingOptions and our UUID class is an internal concept that we don't
  // want to expose to clients.
  typedef std::map<string, string> FastAdvertisementServiceUUIDMap;
  FastAdvertisementServiceUUIDMap fast_advertisement_service_uuids_;

  // ------------ ADVERTISEMENT HEADER MAPS ------------

  // Maps advertisement headers to AdvertisementReadResults. Tells us when to
  // retry reading a GATT advertisement. If no entry exists for a particular
  // header, we should try reading a GATT advertisement. Entries are added
  // whenever a GATT advertisement read is attempted, and removed when GATT
  // advertisements are lost. Entries are also removed whenever
  // gattAdvertisements removes its entry.
  //
  // The map is also cleared whenever startTracking is called, due to client
  // changes. For example, say clients A and B start scanning and discover
  // advertisements A and B (for both clients) on advertisement header 1. Then,
  // A restarts scanning, causing us to clear stale advertisement A. However,
  // since B was still scanning, we don't remove advertisement header 1 from the
  // map. This causes us to never re-read advertisement A.
  typedef std::map</* RefCounted */ ConstPtr<BLEAdvertisementHeader>,
                   Ptr<AdvertisementReadResult<Platform>>>
      AdvertisementReadResultMap;
  AdvertisementReadResultMap advertisement_read_results_;

  // Maps advertisement headers to a set of GATT advertisements from a single
  // peripheral. Used to retrieve GATT advertisements that we need to reprocess
  // every time a header is seen. Entries are added when GATT advertisements are
  // read, removed when all associated GATT advertisements are lost or become
  // stale, and replaced when the advertisement header is updated for a single
  // remote peripheral.
  typedef std::set</* RefCounted */ ConstPtr<BLEAdvertisement>>
      BLEAdvertisementSet;
  typedef std::map</* RefCounted */ ConstPtr<BLEAdvertisementHeader>,
                   Ptr<BLEAdvertisementSet>>
      GattAdvertisementMap;
  GattAdvertisementMap gatt_advertisements_;

  // ------------ GATT ADVERTISEMENT MAPS ------------
  // Entries in these maps all follow the same lifecycle. Entries are added when
  // GATT advertisements are read, and removed when GATT advertisements are lost
  // or become stale.

  // Maps GATT advertisements to the service ID it's associated with. Tracks
  // what GATT advertisements are currently active. Used to determine which
  // LostEntityTracker to invoke when advertisements are rediscovered.
  typedef std::map</* RefCounted */ ConstPtr<BLEAdvertisement>, string>
      AdvertisementServiceIdMap;
  AdvertisementServiceIdMap advertisement_service_ids_;

  // Maps GATT advertisements to advertisement headers. Used to efficiently find
  // advertisement headers to delete when GATT advertisements are updated. This
  // is a reverse map of gatt_advertisements_.
  typedef std::map</* RefCounted */ ConstPtr<BLEAdvertisement>,
                   /* RefCounted */ ConstPtr<BLEAdvertisementHeader>>
      AdvertisementHeaderMap;
  AdvertisementHeaderMap advertisement_headers_;

  // Maps GATT advertisements to MAC addresses. Used when we need to make a
  // socket connection based off of the GATT advertisement alone. Entries are
  // modified every time a GATT advertisement's advertisement header is seen.
  typedef std::map</* RefCounted */ ConstPtr<BLEAdvertisement>, string>
      MacAddressMap;
  MacAddressMap mac_addresses_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/discovered_peripheral_tracker.cc"

#endif  // CORE_INTERNAL_MEDIUMS_DISCOVERED_PERIPHERAL_TRACKER_H_
