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

#include "core/internal/mediums/discovered_peripheral_tracker.h"

#include "core/internal/mediums/ble_packet.h"
#include "core/internal/mediums/bloom_filter.h"
#include "core/internal/mediums/utils.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace dpt {

template <typename K, typename V>
void eraseOwnedPtrFromMap(std::map<K, V>& m, const K& k) {
  typename std::map<K, V>::iterator it = m.find(k);
  if (it != m.end()) {
    it->second.destroy();
    m.erase(it);
  }
}

template <typename K, typename V>
void eraseAllOwnedPtrsFromMap(std::map<K, Ptr<V>>& m) {
  for (typename std::map<K, Ptr<V>>::iterator it = m.begin(); it != m.end();
       ++it) {
    it->second.destroy();
  }
  m.clear();
}

template <typename K, typename V>
V removeOwnedPtrFromMap(std::map<K, V>& m, const K& k) {
  V removed_ptr;
  typename std::map<K, V>::iterator it = m.find(k);
  if (it != m.end()) {
    removed_ptr = it->second;
    m.erase(it);
  }
  return removed_ptr;
}

}  // namespace dpt

// The maximum number of advertisement slots to assume if we don't know the
// exact number.
template <typename Platform>
const std::int32_t DiscoveredPeripheralTracker<Platform>::kMaxSlots = 10;

// Amount of time to wait before attempting a connection. This is needed to
// prevent the GATT server from operation overload if we just came from a GATT
// discovery.
template <typename Platform>
const std::int64_t
    DiscoveredPeripheralTracker<Platform>::kMinConnectionDelayMillis =
        5 * 1000;  // 5 seconds

template <typename Platform>
const char* DiscoveredPeripheralTracker<Platform>::kCopresenceServiceUuid =
    "0000FEF3-0000-1000-8000-00805F9B34FB";

template <typename Platform>
DiscoveredPeripheralTracker<Platform>::DiscoveredPeripheralTracker()
    : lock_(Platform::createLock()),
      thread_utils_(Platform::createThreadUtils()),
      system_clock_(Platform::createSystemClock()),
      hash_utils_(Platform::createHashUtils()),
      discovered_peripheral_callbacks_(),
      lost_entity_trackers_(),
      fast_advertisement_service_uuids_(),
      advertisement_read_results_(),
      gatt_advertisements_(),
      advertisement_service_ids_(),
      advertisement_headers_(),
      mac_addresses_() {}

template <typename Platform>
DiscoveredPeripheralTracker<Platform>::~DiscoveredPeripheralTracker() {
  Synchronized s(lock_.get());

  mac_addresses_.clear();
  advertisement_headers_.clear();
  advertisement_service_ids_.clear();
  // gatt_advertisements_ maps a string to a Ptr to a set of ConstPtrs. We do
  // not go and iterate through every set because those values are RefCounted.
  dpt::eraseAllOwnedPtrsFromMap(gatt_advertisements_);
  dpt::eraseAllOwnedPtrsFromMap(advertisement_read_results_);
  fast_advertisement_service_uuids_.clear();
  dpt::eraseAllOwnedPtrsFromMap(lost_entity_trackers_);
  dpt::eraseAllOwnedPtrsFromMap(discovered_peripheral_callbacks_);
}

// Starts tracking discoveries for a particular service ID.
template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::startTracking(
    const string& service_id,
    Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback,
    const string& fast_advertisement_service_uuid) {
  Synchronized s(lock_.get());

  dpt::eraseOwnedPtrFromMap(discovered_peripheral_callbacks_, service_id);
  discovered_peripheral_callbacks_.insert(
      std::make_pair(service_id, discovered_peripheral_callback));

  // We create a new LostEntityTracker because any pre-existing ones only
  // contain stale advertisements. LostEntityTracker also doesn't provide a
  // reset method, so creating a new one is the right way to go.
  dpt::eraseOwnedPtrFromMap(lost_entity_trackers_, service_id);
  lost_entity_trackers_.insert(std::make_pair(
      service_id,
      MakePtr(new LostEntityTracker<Platform, BLEAdvertisement>())));

  if (!fast_advertisement_service_uuid.empty()) {
    fast_advertisement_service_uuids_.erase(service_id);
    fast_advertisement_service_uuids_.insert(
        std::make_pair(service_id, fast_advertisement_service_uuid));
  }

  // Clear all of the GATT read results. With this cleared, we will now attempt
  // to reconnect to every peripheral we see, giving us a chance to search for
  // the new service we're now tracking.
  // See the documentation of advertisementReadResults for more information.
  dpt::eraseAllOwnedPtrsFromMap(advertisement_read_results_);

  // Remove stale data from any previous sessions.
  clearDataForServiceId(service_id);
}

// Stops tracking discoveries for a particular service ID.
template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::stopTracking(
    const string& service_id) {
  Synchronized s(lock_.get());

  fast_advertisement_service_uuids_.erase(service_id);
  dpt::eraseOwnedPtrFromMap(lost_entity_trackers_, service_id);
  dpt::eraseOwnedPtrFromMap(discovered_peripheral_callbacks_, service_id);
}

// Processes a found BLE advertisement.
template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::processFoundBleAdvertisement(
    Ptr<BLEPeripheralV2> ble_peripheral,
    ConstPtr<BLEAdvertisementData> advertisement_data,
    Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<ConstPtr<BLEAdvertisementData>> scoped_advertisement_data(
      advertisement_data);
  ScopedPtr<Ptr<GattAdvertisementFetcher>> scoped_gatt_advertisement_fetcher(
      gatt_advertisement_fetcher);

  if (getTrackedServiceIds().empty()) {
    // TODO(ahlee) logger.atVerbose().log("Ignoring BLE advertisement header
    // because we are not tracking any service IDs.");
    return;
  }

  if (ble_peripheral.isNull() || scoped_advertisement_data.isNull()) {
    // TODO(ahlee) logger.atVerbose().log("Ignoring BLE advertisement header
    // because the given BleSighting is null or incomplete.");
    return;
  }

  handleFastAdvertisement(ble_peripheral, scoped_advertisement_data.get());
  handleAdvertisementHeader(ble_peripheral, scoped_advertisement_data.get(),
                            scoped_gatt_advertisement_fetcher.get());
}

// Processes the set of lost GATT advertisements and notifies the client of any
// lost peripherals.
template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::processLostGattAdvertisements() {
  Synchronized s(lock_.get());

  std::set<string> tracked_service_ids = getTrackedServiceIds();
  for (typename std::set<string>::iterator tsi_it = tracked_service_ids.begin();
       tsi_it != tracked_service_ids.end(); ++tsi_it) {
    BLEAdvertisementSet lost_gatt_advertisements =
        lost_entity_trackers_.find(*tsi_it)->second->computeLostEntities();

    // Clear the map state for each lost GATT advertisement and report it to the
    // client.
    for (BLEAdvertisementSet::iterator lga_it =
             lost_gatt_advertisements.begin();
         lga_it != lost_gatt_advertisements.end(); ++lga_it) {
      clearGattAdvertisement(*lga_it);
      discovered_peripheral_callbacks_.find(*tsi_it)->second->onPeripheralLost(
          generateBlePeripheral(*lga_it), *tsi_it);
    }
  }
}

template <typename Platform>
Ptr<BLEPeripheral> DiscoveredPeripheralTracker<Platform>::generateBlePeripheral(
    ConstPtr<BLEAdvertisement> gatt_advertisement) {
  // TODO(ahlee): Reminder to port over deviceToken change.
  return MakePtr(new BLEPeripheral(BLEAdvertisement::toBytes(
      gatt_advertisement->getVersion(), gatt_advertisement->getSocketVersion(),
      gatt_advertisement->getServiceIdHash(), gatt_advertisement->getData())));
}

template <typename Platform>
std::set<string> DiscoveredPeripheralTracker<Platform>::getTrackedServiceIds() {
  std::set<string> tracked_service_ids;
  for (DiscoveredPeripheralCallbackMap::iterator dpc_it =
           discovered_peripheral_callbacks_.begin();
       dpc_it != discovered_peripheral_callbacks_.end(); ++dpc_it) {
    tracked_service_ids.insert(dpc_it->first);
  }
  return tracked_service_ids;
}

// Note: There is no C++ equivalent for getTrackedGattAdvertisements() because
// we make a copy of the subset of the keys in directly in
// clearDataForServiceId().

template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::clearDataForServiceId(
    const string& service_id) {
  BLEAdvertisementSet gatt_advertisements_to_clear;
  for (AdvertisementServiceIdMap::iterator it =
           advertisement_service_ids_.begin();
       it != advertisement_service_ids_.end(); ++it) {
    if (it->second != service_id) {
      continue;
    }
    gatt_advertisements_to_clear.insert(it->first);
  }

  for (BLEAdvertisementSet::iterator it = gatt_advertisements_to_clear.begin();
       it != gatt_advertisements_to_clear.end(); ++it) {
    clearGattAdvertisement(*it);
  }
}

// Clears out all data related to the provided GATT advertisement. This
// includes:
//   1. Removing the GATT advertisement from GATT advertisement keyed maps. This
//      includes advertisementServiceIds, AdvertisementHeaders, and
//      macAddresses.
//   2. Removing the corresponding advertisement header from
//      advertisementReadResults.
//   3. Removing the corresponding advertisement header from gattAdvertisements,
//      only if there are no remaining GATT advertisements related to that
//      header.
template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::clearGattAdvertisement(
    ConstPtr<BLEAdvertisement> gatt_advertisement) {
  // BLEAdvertisement is RefCounted, so it does not need to be scoped.
  advertisement_service_ids_.erase(gatt_advertisement);
  mac_addresses_.erase(gatt_advertisement);

  ConstPtr<BLEAdvertisementHeader> advertisement_header =
      dpt::removeOwnedPtrFromMap(advertisement_headers_, gatt_advertisement);
  typename GattAdvertisementMap::iterator ga_it =
      gatt_advertisements_.find(advertisement_header);
  if (ga_it != gatt_advertisements_.end()) {
    // Remove the GATT advertisement from the advertisement header it's
    // associated with.
    Ptr<BLEAdvertisementSet> header_gatt_advertisements = ga_it->second;
    header_gatt_advertisements->erase(gatt_advertisement);

    // Unconditionally remove the header from advertisementReadResults so we
    // can attempt to reread the GATT advertisement if they return.
    dpt::eraseOwnedPtrFromMap(advertisement_read_results_,
                              advertisement_header);

    // If there are no more tracked GATT advertisements under this header, go
    // ahead and remove it from gattAdvertisements.
    if (header_gatt_advertisements->empty()) {
      dpt::eraseOwnedPtrFromMap(gatt_advertisements_, advertisement_header);
    }
  }
}

template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::handleFastAdvertisement(
    Ptr<BLEPeripheralV2> ble_peripheral,
    ConstPtr<BLEAdvertisementData> advertisement_data) {
  // Extract the fast advertisement bytes, if any.
  ScopedPtr<ConstPtr<ByteArray>> fast_advertisement_bytes(
      extractFastAdvertisementBytes(advertisement_data));
  if (fast_advertisement_bytes.isNull()) {
    return;
  }

  // Create a header tied to this fast advertisement. This helps us track the
  // advertisement when reporting it as lost or connecting.
  /* RefCounted */ ConstPtr<BLEAdvertisementHeader> fast_advertisement_header =
      createFastAdvertisementHeader(fast_advertisement_bytes.get());

  // Process the fast advertisement like we would a GATT advertisement and
  // insert a placeholder AdvertisementReadResult.
  dpt::eraseOwnedPtrFromMap(advertisement_read_results_,
                            fast_advertisement_header);
  advertisement_read_results_.insert(
      std::make_pair(fast_advertisement_header,
                     MakePtr(new AdvertisementReadResult<Platform>())));

  std::set<ConstPtr<ByteArray>> fast_advertisement_bytes_set;
  fast_advertisement_bytes_set.insert(fast_advertisement_bytes.get());
  handleRawGattAdvertisements(fast_advertisement_header,
                              fast_advertisement_bytes_set,
                              /* are_fast_advertisements= */ true);
  updateCommonStateForFoundBleAdvertisement(fast_advertisement_header,
                                            ble_peripheral->getId());
}

template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::handleAdvertisementHeader(
    Ptr<BLEPeripheralV2> ble_peripheral,
    ConstPtr<BLEAdvertisementData> advertisement_data,
    Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher) {
  // Attempt to parse the advertisement header.
  /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header =
      BLEAdvertisementHeader::fromString(
          extractAdvertisementHeaderBytes(ble_peripheral, advertisement_data));
  if (advertisement_header.isNull()) {
    // TODO(ahlee) logger.atVerbose().log("Failed to deserialize BLE
    // advertisement header %s. Ignoring.",
    // bytesToString(advertisementHeaderBytes));
    return;
  }

  // Check if the advertisement header contains a service ID we're tracking.
  if (!isInterestingAdvertisementHeader(advertisement_header)) {
    // TODO(ahlee) logger.atVerbose().log("Ignoring BLE advertisement header %s
    // because it does not contain any service IDs we're interested in.",
    // advertisementHeader);
    return;
  }

  // Determine whether or not we need to read a fresh GATT advertisement.
  if (shouldReadFromAdvertisementGattServer(advertisement_header)) {
    // Determine whether or not we need to read a fresh GATT advertisement.
    std::set<ConstPtr<ByteArray>> raw_gatt_advertisements =
        fetchRawGattAdvertisements(ble_peripheral, advertisement_header,
                                   gatt_advertisement_fetcher);
    if (!raw_gatt_advertisements.empty()) {
      handleRawGattAdvertisements(advertisement_header, raw_gatt_advertisements,
                                  /* are_fast_advertisements= */ false);
    }
  }

  // Regardless of whether or not we read a new GATT advertisement, the maps
  // should now be up-to-date. With this information, do some general
  // housekeeping.
  updateCommonStateForFoundBleAdvertisement(
      advertisement_header, /* mac_address= */ ble_peripheral->getId());
}

template <typename Platform>
string DiscoveredPeripheralTracker<Platform>::extractAdvertisementHeaderBytes(
    Ptr<BLEPeripheralV2> ble_peripheral,
    ConstPtr<BLEAdvertisementData> advertisement_data) {
  ConstPtr<ByteArray> service_data;
  std::map<string, ConstPtr<ByteArray>>::const_iterator sd_it =
      advertisement_data->service_data.find(kCopresenceServiceUuid);
  if (sd_it != advertisement_data->service_data.end()) {
    service_data = sd_it->second;
  }
  const string& local_name = advertisement_data->local_name;  // alias

  // A valid advertisement header lives in either the local name (iOS) or the
  // service data (Android).
  if (!service_data.isNull()) {
    // TODO(ahlee) logger.atVerbose().log("Service data found on possible
    // Android BLE peripheral at address %s",
    // bleSighting.getDevice().getAddress());
    return string(service_data->getData(), service_data->size());
  } else if (!local_name.empty()) {
    // TODO(ahlee) logger.atVerbose().log("Local name found on possible iOS BLE
    // peripheral at address %s", bleSighting.getDevice().getAddress());
    return local_name;
  } else {
    // iOS peripherals have a bug where the local name sometimes doesn't appear.
    // In that case, we should still take a look at the advertisement in case
    // there's something valuable on the peripheral's GATT server.

    // TODO(ahlee) logger.atVerbose().log("BLE advertisement found with no
    // service data or local name from BLE peripheral at address %s (could be a
    // buggy iOS peripheral with a missing local name).",
    // bleSighting.getDevice().getAddress());

    // Create a phony BloomFilter that always contains the service ID we're
    // looking for.
    return createDummyAdvertisementHeaderBytes(ble_peripheral);
  }
}

template <typename Platform>
ConstPtr<ByteArray>
DiscoveredPeripheralTracker<Platform>::extractFastAdvertisementBytes(
    ConstPtr<BLEAdvertisementData> advertisement_data) {
  ConstPtr<ByteArray> fast_advertisement_bytes;
  // Iterate through all tracked service IDs to see if any of their fast
  // advertisements are contained within this BLE advertisement.
  std::set<string> tracked_service_ids = getTrackedServiceIds();
  for (typename std::set<string>::iterator tsi_it = tracked_service_ids.begin();
       tsi_it != tracked_service_ids.end(); ++tsi_it) {
    // First, check if a service UUID is tied to this service ID.
    typename FastAdvertisementServiceUUIDMap::iterator fasu_it =
        fast_advertisement_service_uuids_.find(*tsi_it);
    if (fasu_it != fast_advertisement_service_uuids_.end()) {
      const string& fast_advertisement_service_uuid = fasu_it->second;  // alias

      // Then, check if there's service data for this fast advertisement
      // service UUID. If so, we can short-circuit since all BLE
      // advertisements can contain at most ONE fast advertisement.
      typename std::map<string, ConstPtr<ByteArray>>::const_iterator sd_it =
          advertisement_data->service_data.find(
              fast_advertisement_service_uuid);
      if (sd_it != advertisement_data->service_data.end()) {
        // TODO(b/117432693): Remove this copy once Ptr is fully RefCounted.
        fast_advertisement_bytes = MakeConstPtr(
            new ByteArray(sd_it->second->getData(), sd_it->second->size()));
        break;
      }
    }
  }
  return fast_advertisement_bytes;
}

// Creates an advertisement header that's purely a hash of the fast
// advertisement, since they come with no header.
template <typename Platform>
/* RefCounted */ ConstPtr<BLEAdvertisementHeader>
DiscoveredPeripheralTracker<Platform>::createFastAdvertisementHeader(
    ConstPtr<ByteArray> fast_advertisement_bytes) {
  // Our end goal is to have a fully zeroed-out byte array of the correct length
  // representing an empty bloom filter.
  // TODO(b/149938110): remove ScopedPtr.
  ScopedPtr<ConstPtr<ByteArray>> bloom_filter_bytes{ConstPtr<ByteArray>{
      new ByteArray{BLEAdvertisementHeader::kServiceIdBloomFilterLength}}};

  ScopedPtr<ConstPtr<ByteArray>> advertisement_hash(
      generateAdvertisementHash(fast_advertisement_bytes));
  return MakeRefCountedConstPtr(new BLEAdvertisementHeader(
      BLEAdvertisementHeader::Version::V2, /* num_slots= */ 1,
      bloom_filter_bytes.get(), advertisement_hash.get()));
}

// Creates a dummy advertisement header that possibly contains all tracked
// service IDs.
template <typename Platform>
string
DiscoveredPeripheralTracker<Platform>::createDummyAdvertisementHeaderBytes(
    Ptr<BLEPeripheralV2> ble_peripheral) {
  // Put the service ID along with the dummy service ID into our bloom filter
  // Note: BloomFilter length should always match
  // BLEAdvertisementHeader::kServiceIdBloomFilterLength
  ScopedPtr<Ptr<BloomFilter<10>>> bloom_filter(new BloomFilter<10>());

  std::set<string> tracked_service_ids = getTrackedServiceIds();
  for (typename std::set<string>::iterator tsi_it = tracked_service_ids.begin();
       tsi_it != tracked_service_ids.end(); ++tsi_it) {
    bloom_filter->add(*tsi_it);
  }

  const string& ble_peripheral_id = ble_peripheral->getId();  // alias
  ScopedPtr<ConstPtr<ByteArray>> ble_peripheral_id_bytes(MakeConstPtr(
      new ByteArray(ble_peripheral_id.data(), ble_peripheral_id.size())));
  ScopedPtr<ConstPtr<ByteArray>> advertisement_hash(
      generateAdvertisementHash(ble_peripheral_id_bytes.get()));
  return BLEAdvertisementHeader::asString(BLEAdvertisementHeader::Version::V2,
                                          kMaxSlots, bloom_filter->asBytes(),
                                          advertisement_hash.get());
}

template <typename Platform>
bool DiscoveredPeripheralTracker<Platform>::isInterestingAdvertisementHeader(
    /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header) {
  ScopedPtr<Ptr<BloomFilter<10>>> bloom_filter(
      new BloomFilter<10>(advertisement_header->getServiceIdBloomFilter()));
  std::set<string> tracked_service_ids = getTrackedServiceIds();
  for (typename std::set<string>::iterator tsi_it = tracked_service_ids.begin();
       tsi_it != tracked_service_ids.end(); ++tsi_it) {
    if (bloom_filter->possiblyContains(*tsi_it)) {
      return true;
    }
  }
  return false;
}

template <typename Platform>
bool DiscoveredPeripheralTracker<Platform>::
    shouldReadFromAdvertisementGattServer(
        /* RefCounted */ ConstPtr<BLEAdvertisementHeader>
            advertisement_header) {
  // Check if we have never seen this header. New headers should always be read.
  typename AdvertisementReadResultMap::iterator arr_it =
      advertisement_read_results_.find(advertisement_header);
  if (arr_it == advertisement_read_results_.end()) {
    // TODO(ahlee) logger.atDebug().log("Received advertisement header %s, but
    // we have never seen it before. Will try reading its GATT advertisement.",
    // advertisementHeader);
    return true;
  }

  // Extract the last read result for this particular header.
  Ptr<AdvertisementReadResult<Platform>> advertisement_read_result =
      arr_it->second;  // alias

  // Now evaluate if we should retry reading.
  switch (advertisement_read_result->evaluateRetryStatus()) {
    case AdvertisementReadResult<Platform>::RetryStatus::RETRY:
      // TODO(ahlee) logger.atDebug().log("Received advertisement header %s.
      // Will retry reading its GATT advertisement.", advertisementHeader);
      return true;
    case AdvertisementReadResult<Platform>::RetryStatus::PREVIOUSLY_SUCCEEDED:
      // TODO(ahlee) logger.atVerbose().log("Received advertisement header %s,
      // but we have already read its GATT advertisement.",
      // advertisementHeader);
      return false;
    case AdvertisementReadResult<Platform>::RetryStatus::TOO_SOON:
      // TODO(ahlee) logger.atDebug().log("Received advertisement header %s, but
      // we have recently failed to read its GATT advertisement.",
      // advertisementHeader);
      return false;
    case AdvertisementReadResult<Platform>::RetryStatus::UNKNOWN:
      // Fall through.
      break;
  }

  // TODO(ahlee) logger.atDebug().log("Received advertisement header %s, but we
  // do not know whether or not to retry reading its GATT advertisement. Will
  // retry to be safe.", advertisementHeader);
  return true;
}

template <typename Platform>
std::set<ConstPtr<ByteArray>>
DiscoveredPeripheralTracker<Platform>::fetchRawGattAdvertisements(
    Ptr<BLEPeripheralV2> ble_peripheral,
    /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
    Ptr<GattAdvertisementFetcher> gatt_advertisement_fetcher) {
  Ptr<AdvertisementReadResult<Platform>> old_advertisement_read_result;
  typename AdvertisementReadResultMap::iterator arr_it =
      advertisement_read_results_.find(advertisement_header);
  if (arr_it != advertisement_read_results_.end()) {
    old_advertisement_read_result = arr_it->second;  // alias
  }

  /* RefCounted */ Ptr<AdvertisementReadResult<Platform>>
      advertisement_read_result =
          gatt_advertisement_fetcher->fetchGattAdvertisements(
              ble_peripheral, advertisement_header->getNumSlots(),
              old_advertisement_read_result);

  dpt::eraseOwnedPtrFromMap(advertisement_read_results_, advertisement_header);
  arr_it = advertisement_read_results_
               .insert(std::make_pair(advertisement_header,
                                      advertisement_read_result))
               .first;

  return arr_it->second->getAdvertisements();
}

template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::handleRawGattAdvertisements(
    /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
    const std::set<ConstPtr<ByteArray>>& raw_gatt_advertisements,
    bool are_fast_advertisements) {
  typedef std::map<string, ConstPtr<BLEAdvertisement>> BLEAdvertisementMap;
  // Parse the raw GATT advertisements. The output of this method is a mapping
  // of service ID -> GATT advertisement.
  BLEAdvertisementMap parsed_gatt_advertisements =
      parseRawGattAdvertisements(raw_gatt_advertisements);
  ScopedPtr<Ptr<BLEAdvertisementSet>> parsed_gatt_advertisement_values(
      new BLEAdvertisementSet());

  // Update state for each GATT advertisement.
  for (BLEAdvertisementMap::iterator pga_it =
           parsed_gatt_advertisements.begin();
       pga_it != parsed_gatt_advertisements.end(); ++pga_it) {
    const string& service_id = pga_it->first;                        // alias
    ConstPtr<BLEAdvertisement> gatt_advertisement = pga_it->second;  // alias
    parsed_gatt_advertisement_values->insert(gatt_advertisement);

    // TODO(ahlee): Update the java code to create old_advertisement_header
    // within the if/else block.
    AdvertisementHeaderMap::iterator ah_it =
        advertisement_headers_.find(gatt_advertisement);
    if (ah_it == advertisement_headers_.end()) {
      discovered_peripheral_callbacks_.find(service_id)
          ->second->onPeripheralDiscovered(
              generateBlePeripheral(gatt_advertisement), service_id,
              gatt_advertisement->getData(), are_fast_advertisements);
    } else {
      ConstPtr<BLEAdvertisementHeader> old_advertisement_header =
          ah_it->second;  // alias
      dpt::eraseOwnedPtrFromMap(advertisement_read_results_,
                                old_advertisement_header);
      dpt::eraseOwnedPtrFromMap(gatt_advertisements_, old_advertisement_header);
    }

    dpt::eraseOwnedPtrFromMap(advertisement_headers_, gatt_advertisement);
    advertisement_headers_.insert(
        std::make_pair(gatt_advertisement, advertisement_header));

    advertisement_service_ids_.erase(gatt_advertisement);
    advertisement_service_ids_.insert(
        std::make_pair(gatt_advertisement, service_id));
  }

  // Insert the list of read GATT advertisements for this advertisement header.
  dpt::eraseOwnedPtrFromMap(gatt_advertisements_, advertisement_header);
  gatt_advertisements_.insert(std::make_pair(
      advertisement_header, parsed_gatt_advertisement_values.release()));
}

// Returns a map of service IDs to GATT advertisements who belong to a tracked
// service ID.
template <typename Platform>
std::map<string, ConstPtr<BLEAdvertisement>>
DiscoveredPeripheralTracker<Platform>::parseRawGattAdvertisements(
    const std::set<ConstPtr<ByteArray>>& raw_gatt_advertisements) {
  std::set<string> tracked_service_ids = getTrackedServiceIds();
  typedef std::map<string, ConstPtr<BLEAdvertisement>> BLEAdvertisementMap;
  BLEAdvertisementMap parsed_gatt_advertisements;
  for (std::set<ConstPtr<ByteArray>>::iterator rga_it =
           raw_gatt_advertisements.begin();
       rga_it != raw_gatt_advertisements.end(); ++rga_it) {
    /* RefCounted */ ConstPtr<BLEAdvertisement> gatt_advertisement =
        BLEAdvertisement::fromBytes(*rga_it);
    if (gatt_advertisement.isNull()) {
      // logger.atDebug().log("Unable to parse raw GATT advertisement %s",
      // *rga_it);
      continue;
    }

    // Make sure the advertisement belongs to a service ID we're tracking.
    for (typename std::set<string>::iterator tsi_it =
             tracked_service_ids.begin();
         tsi_it != tracked_service_ids.end(); ++tsi_it) {
      // If we already found a higher version advertisement for this service ID,
      // there's no point in comparing this advertisement against it.
      BLEAdvertisementMap::iterator pga_it =
          parsed_gatt_advertisements.find(*tsi_it);
      if (pga_it != parsed_gatt_advertisements.end()) {
        if (pga_it->second->getVersion() > gatt_advertisement->getVersion()) {
          continue;
        }
      }

      // Map the service ID to the advertisement if the service ID hashes match.
      ScopedPtr<ConstPtr<ByteArray>> service_id_hash(
          generateServiceIdHash(gatt_advertisement->getVersion(), *tsi_it));
      if (*service_id_hash == *(gatt_advertisement->getServiceIdHash())) {
        // logger.atDebug().log("Matched service ID %s to GATT advertisement
        // %s.", serviceId, gattAdvertisement);
        parsed_gatt_advertisements.insert(
            std::make_pair(*tsi_it, gatt_advertisement));
        break;
      }
    }
  }

  return parsed_gatt_advertisements;
}

template <typename Platform>
void DiscoveredPeripheralTracker<Platform>::
    updateCommonStateForFoundBleAdvertisement(
        /* RefCounted */ ConstPtr<BLEAdvertisementHeader> advertisement_header,
        const string& mac_address) {
  typename GattAdvertisementMap::iterator ga_it =
      gatt_advertisements_.find(advertisement_header);
  if (ga_it == gatt_advertisements_.end()) {
    // logger.atDebug().log("No GATT advertisements found for advertisement
    // header %s.", advertisementHeader);
    return;
  }

  Ptr<BLEAdvertisementSet> saved_gatt_advertisements = ga_it->second;  // alias
  for (BLEAdvertisementSet::iterator sga_it =
           saved_gatt_advertisements->begin();
       sga_it != saved_gatt_advertisements->end(); ++sga_it) {
    ConstPtr<BLEAdvertisement> gatt_advertisement = *sga_it;  // alias

    AdvertisementServiceIdMap::iterator asi_it =
        advertisement_service_ids_.find(gatt_advertisement);
    if (asi_it == advertisement_service_ids_.end()) {
      continue;
    }
    const string& service_id = asi_it->second;  // alias

    // Make sure the stored GATT advertisement is still being tracked.
    std::set<string> tracked_service_ids = getTrackedServiceIds();
    if (tracked_service_ids.find(service_id) == tracked_service_ids.end()) {
      continue;
    }

    // The iterator returned from find() is guaranteed to be valid because it's
    // tied to discovered_peripheral_callbacks_, whose keyset is checked through
    // getTrackedServiceIds() above.
    lost_entity_trackers_.find(service_id)
        ->second->recordFoundEntity(gatt_advertisement);

    // The iterator returned from find() is guaranteed to be valid because it's
    // tied to advertisement_service_ids_ which is checked at the beginning of
    // the for loop.
    mac_addresses_.erase(gatt_advertisement);
    mac_addresses_.insert(std::make_pair(gatt_advertisement, mac_address));
  }
}

template <typename Platform>
ConstPtr<ByteArray>
DiscoveredPeripheralTracker<Platform>::generateAdvertisementHash(
    ConstPtr<ByteArray> advertisement_bytes) {
  return Utils::sha256Hash(hash_utils_.get(), advertisement_bytes,
                           BLEAdvertisementHeader::kAdvertisementHashLength);
}

template <typename Platform>
ConstPtr<ByteArray>
DiscoveredPeripheralTracker<Platform>::generateServiceIdHash(
    BLEAdvertisement::Version::Value version, const string& service_id) {
  ScopedPtr<ConstPtr<ByteArray>> service_id_bytes(
      MakeConstPtr(new ByteArray(service_id.data(), service_id.size())));
  switch (version) {
    case BLEAdvertisement::Version::V1:
      return Utils::legacySha256HashOnlyForPrinting(
          hash_utils_.get(), service_id_bytes.get(),
          BLEPacket::kServiceIdHashLength);
    case BLEAdvertisement::Version::V2:
      // Fall through.
    case BLEAdvertisement::Version::UNKNOWN:
      // Fall through.
    default:
      // Use the latest known hashing scheme.
      return Utils::sha256Hash(hash_utils_.get(), service_id_bytes.get(),
                               BLEPacket::kServiceIdHashLength);
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
