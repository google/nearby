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

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/ble_v2/instant_on_lost_advertisement.h"
#include "connections/implementation/mediums/lost_entity_tracker.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/uuid.h"

using ::nearby::api::ble_v2::BleAdvertisementData;

namespace nearby {
namespace connections {
namespace mediums {
namespace {
constexpr int kGattThreadCount = 1;
constexpr absl::Duration kInstantLostAdvertisementTimeout = absl::Seconds(60);
}  // namespace

DiscoveredPeripheralTracker::DiscoveredPeripheralTracker(
    bool is_extended_advertisement_available)
    : is_extended_advertisement_available_(
          is_extended_advertisement_available) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableGattQueryInThread)) {
    executor_ = std::make_unique<MultiThreadExecutor>(kGattThreadCount);
  }
}

DiscoveredPeripheralTracker::~DiscoveredPeripheralTracker() {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableGattQueryInThread)) {
    MutexLock lock(&mutex_);
    if (executor_ != nullptr) {
      executor_->Shutdown();
    }
  }
}

void DiscoveredPeripheralTracker::StartTracking(
    const std::string& service_id,
    DiscoveredPeripheralCallback discovered_peripheral_callback,
    const Uuid& fast_advertisement_service_uuid) {
  MutexLock lock(&mutex_);

  ServiceIdInfo service_id_info = {
      .discovered_peripheral_callback =
          std::move(discovered_peripheral_callback),
      .lost_entity_tracker =
          std::make_unique<LostEntityTracker<BleAdvertisement>>(),
      .fast_advertisement_service_uuid = fast_advertisement_service_uuid};

  // Replace if key exists.
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
    BleV2Peripheral peripheral, BleAdvertisementData advertisement_data,
    AdvertisementFetcher advertisement_fetcher) {
  MutexLock lock(&mutex_);

  if (service_id_infos_.empty()) {
    LOG(INFO) << "Ignoring BLE advertisement header because we are not "
                 "tracking any service IDs.";
    return;
  }

  if (!peripheral.IsValid() || advertisement_data.service_data.empty()) {
    LOG(INFO) << "Ignoring BLE advertisement header because the peripheral is "
                 "invalid or the given service data is empty.";
    return;
  }

  if (HandleOnLostAdvertisementLocked(peripheral, advertisement_data)) {
    return;
  }

  if (IsSkippableGattAdvertisement(advertisement_data)) {
    LOG(INFO)
        << "Ignore GATT advertisement and wait for extended advertisement.";
    return;
  }

  if (IsLegacyDeviceAdvertisementData(advertisement_data)) {
    if (nearby::FeatureFlags::GetInstance()
            .GetFlags()
            .enable_invoking_legacy_device_discovered_cb) {
      for (auto& itor : service_id_infos_) {
        itor.second.discovered_peripheral_callback
            .legacy_device_discovered_cb();
      }
    }
    return;
  }
  HandleAdvertisement(peripheral, advertisement_data);
  HandleAdvertisementHeader(peripheral, advertisement_data,
                            std::move(advertisement_fetcher));
}

bool DiscoveredPeripheralTracker::HandleOnLostAdvertisementLocked(
    BleV2Peripheral peripheral,
    const BleAdvertisementData& advertisement_data) {
  auto service_data =
      advertisement_data.service_data.find(bleutils::kCopresenceServiceUuid);
  if (service_data == advertisement_data.service_data.end()) {
    return false;
  }
  absl::StatusOr<InstantOnLostAdvertisement> on_lost_advertisement =
      InstantOnLostAdvertisement::CreateFromBytes(
          service_data->second.AsStringView());
  if (!on_lost_advertisement.ok()) {
    return false;
  }

  LOG(INFO) << __func__ << ": Found OnLost advertisement for hash:"
            << absl::BytesToHexString(on_lost_advertisement->ToBytes());

  for (const auto& hash : on_lost_advertisement->hashes()) {
    for (const auto& it : gatt_advertisement_infos_) {
      if (it.second.advertisement_header.GetAdvertisementHash().string_data() ==
          hash) {
        auto discovery_cb_it = service_id_infos_.find(it.second.service_id);
        if (discovery_cb_it == service_id_infos_.end()) {
          LOG(INFO)
              << __func__
              << ": Discarding OnLost advertisement for untracked service_id";
          break;
        }

        auto gatt_advertisements =
            gatt_advertisements_[it.second.advertisement_header];

        // Need to report OnLost for each gatt_advertisement.
        for (const auto& gatt_advertisement : gatt_advertisements) {
          BleV2Peripheral lost_peripheral = it.second.peripheral;
          lost_peripheral.SetId(ByteArray(gatt_advertisement));
          if (gatt_advertisement.IsValid()) {
            if (NearbyFlags::GetInstance().GetBoolFlag(
                    config_package_nearby::nearby_connections_feature::
                        kEnableInstantOnLost)) {
              AddInstantLostAdvertisement(it.second.advertisement_header);
              discovery_cb_it->second.discovered_peripheral_callback
                  .instant_lost_cb(lost_peripheral, it.second.service_id,
                                   gatt_advertisement.GetData(),
                                   gatt_advertisement.IsFastAdvertisement());
            } else {
              discovery_cb_it->second.discovered_peripheral_callback
                  .peripheral_lost_cb(lost_peripheral, it.second.service_id,
                                      gatt_advertisement.GetData(),
                                      gatt_advertisement.IsFastAdvertisement());
            }
            LOG(INFO) << __func__ << ": OnLost triggered for service_id "
                      << it.second.service_id;
          }

          ClearGattAdvertisement(gatt_advertisement);
        }
        break;
      }
    }
  }
  return true;
}

void DiscoveredPeripheralTracker::ProcessLostGattAdvertisements() {
  MutexLock lock(&mutex_);

  for (auto& it : service_id_infos_) {
    const std::string& service_id = it.first;
    ServiceIdInfo& service_id_info = it.second;

    BleAdvertisementSet lost_gatt_advertisements =
        service_id_info.lost_entity_tracker->ComputeLostEntities();
    // Clear the map state for each lost GATT advertisement and report it to
    // the client.
    for (const auto& gatt_advertisement : lost_gatt_advertisements) {
      const auto it = gatt_advertisement_infos_.find(gatt_advertisement);
      if (it != gatt_advertisement_infos_.end()) {
        BleV2Peripheral lost_peripheral = it->second.peripheral;
        if (lost_peripheral.IsValid()) {
          lost_peripheral.SetId(ByteArray(gatt_advertisement));
          service_id_info.discovered_peripheral_callback.peripheral_lost_cb(
              std::move(lost_peripheral), service_id,
              gatt_advertisement.GetData(),
              gatt_advertisement.IsFastAdvertisement());
        }
      }
      ClearGattAdvertisement(gatt_advertisement);
    }
  }
}

void DiscoveredPeripheralTracker::ClearDataForServiceId(
    const std::string& service_id) {
  std::vector<BleAdvertisement> advertisement_list;
  for (const auto& it : gatt_advertisement_infos_) {
    if (it.second.service_id == service_id) {
      advertisement_list.push_back(it.first);
    }
  }

  for (auto const& advertisement : advertisement_list) {
    ClearGattAdvertisement(advertisement);
  }
}

bool DiscoveredPeripheralTracker::IsSkippableGattAdvertisement(
    const api::ble_v2::BleAdvertisementData& advertisement_data) {
  if (!is_extended_advertisement_available_) {
    // Don't skip any advertisement if the scanner doesn't support extended
    // advertisement.
    return false;
  }

  BleAdvertisementHeader advertisement_header(
      ExtractAdvertisementHeaderBytes(advertisement_data));

  return advertisement_header.IsValid() &&
         advertisement_header.IsSupportExtendedAdvertisement();
}

void DiscoveredPeripheralTracker::ClearGattAdvertisement(
    const BleAdvertisement& gatt_advertisement) {
  const auto gai_it = gatt_advertisement_infos_.find(gatt_advertisement);
  if (gai_it == gatt_advertisement_infos_.end()) {
    return;
  }
  auto item = gatt_advertisement_infos_.extract(gai_it);
  GattAdvertisementInfo& gatt_advertisement_info = item.mapped();

  const auto ga_it =
      gatt_advertisements_.find(gatt_advertisement_info.advertisement_header);
  if (ga_it != gatt_advertisements_.end()) {
    // Remove the GATT advertisement from the advertisement header it's
    // associated with.
    BleAdvertisementSet& gatt_advertisement_set = ga_it->second;
    gatt_advertisement_set.erase(gatt_advertisement);

    // Unconditionally remove the header from advertisement_read_results_ so
    // we can attempt to reread the GATT advertisement if they return.
    advertisement_read_results_.erase(
        gatt_advertisement_info.advertisement_header);

    // If there are no more tracked GATT advertisements under this header, go
    // ahead and remove it from gatt_advertisements_.
    if (gatt_advertisement_set.empty()) {
      gatt_advertisements_.erase(ga_it);
    }
  }
}

void DiscoveredPeripheralTracker::HandleAdvertisement(
    BleV2Peripheral peripheral,
    const nearby::api::ble_v2::BleAdvertisementData& advertisement_data) {
  ByteArray advertisement_bytes =
      ExtractInterestingAdvertisementBytes(advertisement_data);
  if (advertisement_bytes.Empty()) {
    return;
  }

  // The UUID that the Fast/Regular Advertisement was found on. For regular
  // advertisement, it's always kCopresenceServiceUuid. For fast
  // advertisement, we may have below 2 UUIDs: 1. kCopresenceServiceUuid 2.
  // Caller UUID.
  // First filter out kCopresenceServiceUuid and see if any Caller UUID
  // existed; if not then just take the kCopresenceServiceUuid as
  // |service_uuid|.
  absl::flat_hash_map<Uuid, nearby::ByteArray> extracted_uuids;
  // Filter out kCoprsence service uuid.
  std::remove_copy_if(advertisement_data.service_data.begin(),
                      advertisement_data.service_data.end(),
                      std::inserter(extracted_uuids, extracted_uuids.end()),
                      [](const std::pair<const Uuid, nearby::ByteArray>& pair) {
                        return pair.first == bleutils::kCopresenceServiceUuid;
                      });
  Uuid service_uuid;
  if (!extracted_uuids.empty()) {
    service_uuid = extracted_uuids.begin()->first;
  } else {
    service_uuid = bleutils::kCopresenceServiceUuid;
  }

  // Create a header tied to this fast advertisement. This helps us track the
  // advertisement when reporting it as lost or connecting.
  BleAdvertisementHeader advertisement_header =
      CreateAdvertisementHeader(advertisement_bytes);

  // Process the fast advertisement like we would a GATT advertisement and
  // insert a placeholder AdvertisementReadResult.
  advertisement_read_results_.insert(
      {advertisement_header, std::make_unique<AdvertisementReadResult>()});

  BleAdvertisementHeader new_advertisement_header = HandleRawGattAdvertisements(
      peripheral, advertisement_header, {&advertisement_bytes}, service_uuid);
  UpdateCommonStateForFoundBleAdvertisement(new_advertisement_header);
}

ByteArray DiscoveredPeripheralTracker::ExtractInterestingAdvertisementBytes(
    const nearby::api::ble_v2::BleAdvertisementData& advertisement_data) {
  // Iterate through all tracked service IDs to see if any of their fast
  // advertisements are contained within this BLE advertisement.
  for (const auto& item : service_id_infos_) {
    const ServiceIdInfo& service_id_info = item.second;
    // Check if there's service data for this fast advertisement service UUID.
    // If so, we can short-circuit since all BLE advertisements can contain at
    // most ONE fast advertisement.
    const auto sd_it = advertisement_data.service_data.find(
        service_id_info.fast_advertisement_service_uuid);
    if (sd_it != advertisement_data.service_data.end()) {
      return sd_it->second;
    }
  }
  return {};
}

BleAdvertisementHeader DiscoveredPeripheralTracker::CreateAdvertisementHeader(
    const ByteArray& advertisement_bytes) {
  // Our end goal is to have a fully zeroed-out byte array of the correct
  // length representing an empty bloom filter.
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());

  return BleAdvertisementHeader(
      BleAdvertisementHeader::Version::kV2, /*extended_advertisement=*/false,
      /*num_slots=*/1, ByteArray(bloom_filter),
      bleutils::GenerateAdvertisementHash(advertisement_bytes),
      /*psm=*/BleAdvertisementHeader::kDefaultPsmValue);
}

BleAdvertisementHeader DiscoveredPeripheralTracker::HandleRawGattAdvertisements(
    BleV2Peripheral peripheral,
    const BleAdvertisementHeader& advertisement_header,
    const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
    const Uuid& service_uuid) {
  absl::flat_hash_map<std::string, BleAdvertisement>
      parsed_gatt_advertisements = ParseRawGattAdvertisements(
          gatt_advertisement_bytes_list, service_uuid);

  // Update state for each GATT advertisement.
  BleAdvertisementSet ble_advertisement_set;
  BleAdvertisementHeader new_advertisement_header = advertisement_header;
  for (const auto& item : parsed_gatt_advertisements) {
    const std::string& service_id = item.first;
    const BleAdvertisement& gatt_advertisement = item.second;
    BleAdvertisementHeader old_advertisement_header;

    const auto gai_it = gatt_advertisement_infos_.find(gatt_advertisement);
    if (gai_it != gatt_advertisement_infos_.end()) {
      old_advertisement_header = gai_it->second.advertisement_header;
    }

    ble_advertisement_set.insert(gatt_advertisement);

    int new_psm = new_advertisement_header.GetPsm();
    if (gatt_advertisement.GetPsm() !=
            BleAdvertisementHeader::kDefaultPsmValue &&
        gatt_advertisement.GetPsm() != new_psm) {
      new_psm = gatt_advertisement.GetPsm();
      // Also set header with new psm value to compare next time.
      new_advertisement_header.SetPsm(new_psm);
    }

    // If the device received first fast advertisement is legacy one after
    // then received extended one, should replace legacy with extended one
    // which has psm value.
    if (!old_advertisement_header.IsValid() ||
        ShouldNotifyForNewPsm(old_advertisement_header.GetPsm(), new_psm)) {
      // The GATT advertisement has never been seen before. Report it up to
      // the client.
      const auto sii_it = service_id_infos_.find(service_id);
      if (sii_it == service_id_infos_.end()) {
        LOG(WARNING) << "HandleRawGattAdvertisements, failed to find "
                        "callback for service_id="
                     << service_id;
        continue;
      }
      if (peripheral.IsValid()) {
        peripheral.SetPsm(new_psm);
        BleV2Peripheral discovered_peripheral = peripheral;
        discovered_peripheral.SetId(ByteArray(gatt_advertisement));

        if (IsInstantLostAdvertisement(new_advertisement_header)) {
          LOG(INFO) << "Skip the advertisement header with hash "
                    << absl::BytesToHexString(
                           new_advertisement_header.GetAdvertisementHash()
                               .AsStringView())
                    << " due to it was reported lost.";
          continue;
        }

        LOG(INFO)
            << "Report new peripheral for the advertisement header with hash "
            << absl::BytesToHexString(
                   new_advertisement_header.GetAdvertisementHash()
                       .AsStringView())
            << ", IsFastAdvertisement "
            << gatt_advertisement.IsFastAdvertisement();
        sii_it->second.discovered_peripheral_callback.peripheral_discovered_cb(
            std::move(discovered_peripheral), service_id,
            gatt_advertisement.GetData(),
            gatt_advertisement.IsFastAdvertisement());
      }
    } else if (old_advertisement_header.GetPsm() !=
                   BleAdvertisementHeader::kDefaultPsmValue &&
               new_advertisement_header.GetPsm() ==
                   BleAdvertisementHeader::kDefaultPsmValue) {
      // Don't replace it in advertisementHeaders if this one without PSM but
      // older has, it's the case that we received extended fast advertisement
      // after then received legacy one.
      continue;
    } else if (ShouldRemoveHeader(old_advertisement_header,
                                  new_advertisement_header)) {
      // The GATT advertisement has been seen on a different advertisement
      // header. Remove info about the old advertisement header since it's
      // stale now.
      advertisement_read_results_.erase(old_advertisement_header);
      gatt_advertisements_.erase(old_advertisement_header);
    }

    GattAdvertisementInfo gatt_advertisement_info = {
        .service_id = service_id,
        .advertisement_header = new_advertisement_header,
        .peripheral = peripheral};
    gatt_advertisement_infos_.insert_or_assign(
        gatt_advertisement, std::move(gatt_advertisement_info));
  }

  // Insert the list of read GATT advertisements for this advertisement
  // header.
  gatt_advertisements_.insert(
      {new_advertisement_header, std::move(ble_advertisement_set)});
  return new_advertisement_header;
}

absl::flat_hash_map<std::string, BleAdvertisement>
DiscoveredPeripheralTracker::ParseRawGattAdvertisements(
    const std::vector<const ByteArray*>& gatt_advertisement_bytes_list,
    const Uuid& service_uuid) {
  absl::flat_hash_map<std::string, BleAdvertisement>
      parsed_gatt_advertisements = {};

  // TODO(edwinwu): Refactor this big loop as subroutines.
  for (const auto gatt_advertisement_bytes : gatt_advertisement_bytes_list) {
    // First, parse the raw bytes into a BleAdvertisement.

    auto gatt_advertisement_status_or =
        BleAdvertisement::CreateBleAdvertisement(*gatt_advertisement_bytes);
    if (!gatt_advertisement_status_or.ok()) {
      VLOG(1) << gatt_advertisement_status_or.status();
      continue;
    }
    auto gatt_advertisement = gatt_advertisement_status_or.value();

    // Make sure the advertisement belongs to a service ID we're tracking.
    for (const auto& item : service_id_infos_) {
      const std::string& service_id = item.first;
      // If we already found a higher version advertisement for this service
      // ID, there's no point in comparing this advertisement against it.
      const auto pga_it = parsed_gatt_advertisements.find(service_id);
      if (pga_it != parsed_gatt_advertisements.end()) {
        if (pga_it->second.GetVersion() > gatt_advertisement.GetVersion()) {
          continue;
        }
      }

      // service_id_hash is null here (mediums advertisement) because we
      // already have a UUID in the fast advertisement.
      if (gatt_advertisement.IsFastAdvertisement() && !service_uuid.IsEmpty()) {
        const auto sii_it = service_id_infos_.find(service_id);
        if (sii_it != service_id_infos_.end()) {
          if (sii_it->second.fast_advertisement_service_uuid == service_uuid) {
            VLOG(1) << "This GATT advertisement:"
                    << absl::BytesToHexString(
                           gatt_advertisement_bytes->AsStringView())
                    << " is a fast advertisement and matched UUID="
                    << service_uuid.Get16BitAsString()
                    << " in a map with service_id=" << service_id;
            parsed_gatt_advertisements.insert({service_id, gatt_advertisement});
          }
        }
        continue;
      }

      // Map the service ID to the advertisement if the service_id_hash match.
      if (bleutils::GenerateServiceIdHash(service_id) ==
          gatt_advertisement.GetServiceIdHash()) {
        LOG(INFO) << "Matched service_id=" << service_id
                  << " to GATT advertisement="
                  << absl::BytesToHexString(
                         gatt_advertisement_bytes->AsStringView());
        parsed_gatt_advertisements.insert({service_id, gatt_advertisement});
        break;
      }
    }
  }

  return parsed_gatt_advertisements;
}

bool DiscoveredPeripheralTracker::ShouldNotifyForNewPsm(int old_psm,
                                                        int new_psm) const {
  return new_psm != BleAdvertisementHeader::kDefaultPsmValue &&
         old_psm != new_psm;
}

bool DiscoveredPeripheralTracker::ShouldRemoveHeader(
    const BleAdvertisementHeader& old_advertisement_header,
    const BleAdvertisementHeader& new_advertisement_header) {
  if (old_advertisement_header == new_advertisement_header) {
    return false;
  }

  // We received the physical from legacy advertisement and create a mock one
  // when receive a regular advertisement from extended advertisements. Avoid
  // to remove the physical header for the new incoming regular extended
  // advertisement. Otherwise, it make the device to fetch advertisement when
  // received a physical header again.
  if (is_extended_advertisement_available_) {
    if (!IsDummyAdvertisementHeader(old_advertisement_header) &&
        IsDummyAdvertisementHeader(new_advertisement_header)) {
      return false;
    }
  }

  return true;
}

bool DiscoveredPeripheralTracker::IsDummyAdvertisementHeader(
    const BleAdvertisementHeader& advertisement_header) {
  // Do not count advertisementHash and psm value here, for L2CAP feature, the
  // regular advertisement has different value, it will include PSM value if
  // received it from extended advertisement protocol and it will not has PSM
  // value if it fetched from GATT connection.
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());
  return advertisement_header.GetVersion() ==
             BleAdvertisementHeader::Version::kV2 &&
         advertisement_header.GetNumSlots() == 1 &&
         advertisement_header.GetServiceIdBloomFilter() ==
             ByteArray(bloom_filter);
}

void DiscoveredPeripheralTracker::HandleAdvertisementHeader(
    BleV2Peripheral peripheral,
    const nearby::api::ble_v2::BleAdvertisementData& advertisement_data,
    AdvertisementFetcher advertisement_fetcher) {
  // Attempt to parse the advertisement header.
  BleAdvertisementHeader advertisement_header(
      ExtractAdvertisementHeaderBytes(advertisement_data));
  if (!advertisement_header.IsValid()) {
    VLOG(1) << "Failed to deserialize BLE advertisement header. Ignoring.";
    return;
  }

  // Check if the advertisement header contains a service ID we're tracking.
  if (!IsInterestingAdvertisementHeader(advertisement_header)) {
    VLOG(1) << "Ignoring BLE advertisement header with hash"
            << absl::BytesToHexString(
                   advertisement_header.GetAdvertisementHash().AsStringView())
            << " because it does not contain any service IDs "
               "we're interested in.";
    return;
  }

  // Report a nearby legacy device is found when advertisement header doesn't
  // support extended advertisement.
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kDisableBluetoothClassicScanning)) {
    if (!advertisement_header.IsSupportExtendedAdvertisement()) {
      for (auto& item : service_id_infos_) {
        item.second.discovered_peripheral_callback
            .legacy_device_discovered_cb();
      }
    }
  }

  // Determine whether or not we need to read a fresh GATT advertisement.
  if (ShouldReadRawAdvertisementFromServer(advertisement_header)) {
    // Determine whether or not we need to read a fresh GATT advertisement.
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kEnableGattQueryInThread)) {
      VLOG(1) << ": Handle GATT advertisement header with hash "
              << absl::BytesToHexString(
                     advertisement_header.GetAdvertisementHash().AsStringView())
              << " in thread";

      if (!fetching_advertisements_.insert(advertisement_header).second) {
        VLOG(1) << ": Ignore the advertisement header due to it "
                   "is already in fetching.";
        return;
      }

      if (executor_ == nullptr) {
        // The situation happens when flag value changed
        executor_ = std::make_unique<MultiThreadExecutor>(kGattThreadCount);
      }
      executor_->Execute(
          [this, peripheral, advertisement_header,
           advertisement_fetcher = std::move(advertisement_fetcher),
           advertisement_data = std::move(advertisement_data)]() mutable {
            FetchRawAdvertisementsInThread(peripheral, advertisement_header,
                                           std::move(advertisement_fetcher));
          });
      return;
    } else {
      std::vector<const ByteArray*> gatt_advertisement_bytes_list =
          FetchRawAdvertisements(peripheral, advertisement_header,
                                 std::move(advertisement_fetcher));
      if (!gatt_advertisement_bytes_list.empty()) {
        HandleRawGattAdvertisements(peripheral, advertisement_header,
                                    gatt_advertisement_bytes_list,
                                    /*service_uuid=*/{});
      }
    }
  }

  // Regardless of whether or not we read a new GATT advertisement, the maps
  // should now be up-to-date. With this information, do some general
  // housekeeping.
  UpdateCommonStateForFoundBleAdvertisement(advertisement_header);
}

ByteArray DiscoveredPeripheralTracker::ExtractAdvertisementHeaderBytes(
    const nearby::api::ble_v2::BleAdvertisementData& advertisement_data) {
  const auto it =
      advertisement_data.service_data.find(bleutils::kCopresenceServiceUuid);
  if (it != advertisement_data.service_data.end()) {
    const ByteArray& advertisement_header_bytes = it->second;
    if (!advertisement_header_bytes.Empty()) {
      return advertisement_header_bytes;
    }
  }
  return {};
}

bool DiscoveredPeripheralTracker::IsInterestingAdvertisementHeader(
    const BleAdvertisementHeader& advertisement_header) {
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>(),
      advertisement_header.GetServiceIdBloomFilter());

  for (const auto& item : service_id_infos_) {
    const std::string& service_id = item.first;
    if (bloom_filter.PossiblyContains(service_id)) {
      return true;
    }
  }
  return false;
}

bool DiscoveredPeripheralTracker::ShouldReadRawAdvertisementFromServer(
    const BleAdvertisementHeader& advertisement_header) {
  // Check if we have never seen this header. New headers should always be
  // read.
  ByteArray advertisement_header_bytes(advertisement_header);
  const auto it = advertisement_read_results_.find(advertisement_header);
  if (it == advertisement_read_results_.end()) {
    LOG(INFO) << "Received advertisement header with hash "
              << absl::BytesToHexString(
                     advertisement_header.GetAdvertisementHash().AsStringView())
              << ", but we have never seen it before. Caller should "
                 "try reading its GATT advertisement.";
    return true;
  }

  // Extract the last read result for this particular header.
  AdvertisementReadResult* advertisement_read_result = it->second.get();

  // Now evaluate if we should retry reading.
  switch (advertisement_read_result->EvaluateRetryStatus()) {
    case AdvertisementReadResult::RetryStatus::kRetry:
      LOG(INFO)
          << "Received advertisement header with hash "
          << absl::BytesToHexString(
                 advertisement_header.GetAdvertisementHash().AsStringView())
          << ". Caller should retry reading its GATT advertisement.";
      return true;
    case AdvertisementReadResult::RetryStatus::kPreviouslySucceeded:
      VLOG(1) << "Received advertisement header with hash "
              << absl::BytesToHexString(
                     advertisement_header.GetAdvertisementHash().AsStringView())
              << ", but we have already read its GATT advertisement.";
      return false;
    case AdvertisementReadResult::RetryStatus::kTooSoon:
      LOG(INFO)
          << "Received advertisement header with hash "
          << absl::BytesToHexString(
                 advertisement_header.GetAdvertisementHash().AsStringView())
          << ", but we have recently failed to read its GATT advertisement.";
      return false;
    case AdvertisementReadResult::RetryStatus::kUnknown:
      // Fall through.
      break;
  }

  LOG(INFO) << "Received advertisement header with hash "
            << absl::BytesToHexString(
                   advertisement_header.GetAdvertisementHash().AsStringView())
            << ", but we do not know whether or not to retry reading "
               "its GATT advertisement. Caller should retry to be safe.";
  return true;
}

std::vector<const ByteArray*>
DiscoveredPeripheralTracker::FetchRawAdvertisements(
    BleV2Peripheral peripheral,
    const BleAdvertisementHeader& advertisement_header,
    AdvertisementFetcher advertisement_fetcher) {
  // Fetch the raw GATT advertisements and store the results.
  auto& result = advertisement_read_results_[advertisement_header];
  if (result == nullptr) {
    result = std::make_unique<mediums::AdvertisementReadResult>();
  }

  std::vector<std::string> service_ids;
  std::transform(service_id_infos_.begin(), service_id_infos_.end(),
                 std::back_inserter(service_ids),
                 [](auto& kv) { return kv.first; });
  advertisement_fetcher(peripheral, advertisement_header.GetNumSlots(),
                        advertisement_header.GetPsm(), service_ids, *result);

  // Take those results and return all the advertisements we were able to
  // read.
  return result->GetAdvertisements();
}

void DiscoveredPeripheralTracker::FetchRawAdvertisementsInThread(
    BleV2Peripheral peripheral,
    const BleAdvertisementHeader& advertisement_header,
    AdvertisementFetcher advertisement_fetcher) {
  std::vector<std::string> service_ids;

  {
    MutexLock lock(&mutex_);
    if (!IsInterestingAdvertisementHeader(advertisement_header)) {
      LOG(INFO) << ": Ignore to read raw advertisement from server due to it "
                   "is not interesting header now.";
      fetching_advertisements_.erase(advertisement_header);
      return;
    }

    std::transform(service_id_infos_.begin(), service_id_infos_.end(),
                   std::back_inserter(service_ids),
                   [](auto& kv) { return kv.first; });
  }

  auto result = std::make_unique<mediums::AdvertisementReadResult>();
  advertisement_fetcher(peripheral, advertisement_header.GetNumSlots(),
                        advertisement_header.GetPsm(), service_ids, *result);
  {
    MutexLock lock(&mutex_);
    // The fetching process might take a few seconds, and tracking settings
    // could change during that time. We need to double-check if the result
    // is still valid afterward.
    if (!IsInterestingAdvertisementHeader(advertisement_header)) {
      LOG(WARNING)
          << ": Ignore the fetched GATT advertisement from server due to it "
             "is not interesting header now.";
      return;
    }

    auto it = advertisement_read_results_.insert_or_assign(advertisement_header,
                                                           std::move(result));
    std::vector<const ByteArray*> gatt_advertisement_bytes_list =
        it.first->second->GetAdvertisements();

    if (gatt_advertisement_bytes_list.empty() ||
        !IsInterestingAdvertisementHeader(advertisement_header)) {
      fetching_advertisements_.erase(advertisement_header);
      return;
    }

    HandleRawGattAdvertisements(peripheral, advertisement_header,
                                gatt_advertisement_bytes_list,
                                /*service_uuid=*/{});
    UpdateCommonStateForFoundBleAdvertisement(advertisement_header);
    fetching_advertisements_.erase(advertisement_header);
    VLOG(1) << ": Completed to handle GATT advertisement header with hash "
            << absl::BytesToHexString(
                   advertisement_header.GetAdvertisementHash().AsStringView())
            << " in thread";
  }
}

void DiscoveredPeripheralTracker::UpdateCommonStateForFoundBleAdvertisement(
    const BleAdvertisementHeader& advertisement_header) {
  const auto ga_it = gatt_advertisements_.find(advertisement_header);
  if (ga_it == gatt_advertisements_.end()) {
    LOG(INFO)
        << "No GATT advertisements found for advertisement header with hash "
        << absl::BytesToHexString(
               advertisement_header.GetAdvertisementHash().AsStringView());
    return;
  }

  BleAdvertisementSet saved_gatt_advertisements = ga_it->second;
  for (const auto& gatt_advertisement : saved_gatt_advertisements) {
    const auto gai_it = gatt_advertisement_infos_.find(gatt_advertisement);
    if (gai_it == gatt_advertisement_infos_.end()) {
      continue;
    }
    const GattAdvertisementInfo& gatt_advertisement_info = gai_it->second;
    const auto sii_it =
        service_id_infos_.find(gatt_advertisement_info.service_id);
    if (sii_it != service_id_infos_.end()) {
      auto* lost_entity_tracker = sii_it->second.lost_entity_tracker.get();
      if (!lost_entity_tracker) {
        LOG(WARNING) << "UpdateCommonStateForFoundBleAdvertisement, "
                        "failed to find entity "
                        "tracker for service_id="
                     << gatt_advertisement_info.service_id;
        continue;
      }
      lost_entity_tracker->RecordFoundEntity(gatt_advertisement);
    }
  }
}

bool DiscoveredPeripheralTracker::IsLegacyDeviceAdvertisementData(
    const BleAdvertisementData& advertisement_data) {
  return !advertisement_data.is_extended_advertisement &&
         advertisement_data.service_data.size() == 1 &&
         advertisement_data.service_data.find(
             bleutils::kCopresenceServiceUuid) !=
             advertisement_data.service_data.end() &&
         advertisement_data.service_data.at(bleutils::kCopresenceServiceUuid) ==
             ByteArray(DiscoveredPeripheralTracker::kDummyAdvertisementValue);
}

bool DiscoveredPeripheralTracker::IsInstantLostAdvertisement(
    const BleAdvertisementHeader& advertisement_header) {
  RemoveExpiredInstantLostAdvertisements();
  return lost_advertisment_infos_.contains(
      std::string(advertisement_header.GetAdvertisementHash()));
}

void DiscoveredPeripheralTracker::AddInstantLostAdvertisement(
    const BleAdvertisementHeader& advertisement_header) {
  LOG(INFO) << "Add instant lost advertisement header with hash "
            << absl::BytesToHexString(
                   advertisement_header.GetAdvertisementHash().AsStringView());
  lost_advertisment_infos_[std::string(
      advertisement_header.GetAdvertisementHash())] =
      SystemClock::ElapsedRealtime();
}

void DiscoveredPeripheralTracker::RemoveExpiredInstantLostAdvertisements() {
  absl::Time now = SystemClock::ElapsedRealtime();
  if (now - last_lost_info_update_time_ < kInstantLostAdvertisementTimeout) {
    return;
  }

  auto it = lost_advertisment_infos_.begin(),
       end = lost_advertisment_infos_.end();

  LOG(INFO) << "Start to remove expired lost advertisements.";
  int count = 0;
  while (it != end) {
    if (now - it->second >= kInstantLostAdvertisementTimeout) {
      lost_advertisment_infos_.erase(it++);
      ++count;
    } else {
      ++it;
    }
  }

  last_lost_info_update_time_ = now;
  LOG(INFO) << "Removed " << count << " expired lost advertisements.";
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
