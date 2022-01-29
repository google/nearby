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

#include "connections/implementation/mediums/ble_v2.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_peripheral.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/power_level.h"
#include "third_party/nearby/cpp/cal/api/ble.h"
#include "third_party/nearby/cpp/cal/base/ble_types.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

using ::nearby::cal::BleAdvertisementData;
using ::nearby::cal::PowerMode;

BleV2::BleV2(BluetoothRadio& radio)
    : radio_(radio), adapter_(radio_.GetBluetoothAdapter()) {}

bool BleV2::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool BleV2::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
    PowerLevel power_level,
    const std::string& fast_advertisement_service_uuid) {
  MutexLock lock(&mutex_);

  if (advertisement_bytes.Empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to turn on BLE advertising. Empty advertisement data.";
    return false;
  }

  if (advertisement_bytes.size() > kMaxAdvertisementLength) {
    NEARBY_LOG(INFO,
               "Refusing to start BLE advertising because the advertisement "
               "was too long. Expected at most %d bytes but received %d.",
               kMaxAdvertisementLength, advertisement_bytes.size());
    return false;
  }

  if (IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Failed to BLE advertise because we're already advertising.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't start BLE scanning because Bluetooth was never turned on";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't turn on BLE advertising. BLE is not available.";
    return false;
  }

  // Wrap the connections advertisement to the medium advertisement.
  const bool fast_advertisement = !fast_advertisement_service_uuid.empty();
  ByteArray service_id_hash = mediums::BleUtils::GenerateHash(
      service_id, mediums::BleAdvertisement::kServiceIdHashLength);
  ByteArray medium_advertisement_bytes{mediums::BleAdvertisement{
      mediums::BleAdvertisement::Version::kV2,
      mediums::BleAdvertisement::SocketVersion::kV2,
      fast_advertisement ? ByteArray{} : service_id_hash, advertisement_bytes,
      mediums::BleUtils::GenerateDeviceToken()}};
  if (medium_advertisement_bytes.Empty()) {
    NEARBY_LOGS(INFO) << "Failed to BLE advertise because we could not wrap a "
                         "connection advertisement to medium advertisement.";
    return false;
  }

  // Stop any existing advertisement GATT servers. We don't stop it in
  // stopAdvertising() to avoid GATT issues with BLE sockets.
  if (IsAdvertisementGattServerRunningLocked()) {
    StopAdvertisementGattServerLocked();
  }

  // Assemble AdvertisingData and ScanResponseData.
  BleAdvertisementData advertising_data;
  BleAdvertisementData scan_response_data;
  if (fast_advertisement) {
    advertising_data.service_data.insert(
        {fast_advertisement_service_uuid, medium_advertisement_bytes});
    scan_response_data.service_uuids.insert(fast_advertisement_service_uuid);
  } else {
    // Start a GATT server to deliver the full advertisement data. If fail to
    // advertise the header, we must shut this down before the method returns.
    if (!StartAdvertisementGattServerLocked(service_id,
                                            medium_advertisement_bytes)) {
      NEARBY_LOGS(INFO) << "Failed to BLE advertise because the "
                           "advertisement GATT server failed to start.";
      return false;
    }

    ByteArray advertisement_header_bytes(CreateAdvertisementHeader());
    if (advertisement_header_bytes.Empty()) {
      NEARBY_LOGS(INFO) << "Failed to BLE advertise because we could not "
                           "create an advertisement header.";
      // Failed to start BLE advertising, so stop the advertisement GATT
      // server.
      StopAdvertisementGattServerLocked();
      return false;
    }

    advertising_data.is_connectable = true;
    advertising_data.tx_power_level =
        BleAdvertisementData::kUnspecifiedTxPowerLevel;

    scan_response_data.is_connectable = true;
    scan_response_data.tx_power_level =
        BleAdvertisementData::kUnspecifiedTxPowerLevel;
    scan_response_data.service_uuids.insert(
        std::string(kCopresenceServiceUuid));
    scan_response_data.service_data.insert(
        {std::string(kCopresenceServiceUuid), advertisement_header_bytes});
  }

  if (!medium_.StartAdvertising(advertising_data, scan_response_data,
                                PowerLevelToPowerMode(power_level))) {
    NEARBY_LOGS(ERROR)
        << "Failed to turn on BLE advertising with advertisement bytes="
        << absl::BytesToHexString(advertisement_bytes.data())
        << ", size=" << advertisement_bytes.size()
        << ", fast advertisement service uuid="
        << fast_advertisement_service_uuid;

    // If BLE advertising was not successful, stop the advertisement GATT
    // server.
    StopAdvertisementGattServerLocked();
    return false;
  }

  NEARBY_LOGS(INFO) << "Started BLE advertising with advertisement bytes="
                    << absl::BytesToHexString(advertisement_bytes.data())
                    << " for service_id=" << service_id;
  advertising_info_.Add(service_id);
  return true;
}

bool BleV2::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Can't turn off BLE advertising; it is already off";
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned off BLE advertising with service id="
                    << service_id;
  bool can_stop = medium_.StopAdvertising();
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising.
  advertising_info_.Remove(service_id);
  return can_stop;
}

bool BleV2::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool BleV2::StartScanning(const std::string& service_id,
                          DiscoveredPeripheralCallback callback,
                          PowerLevel power_level,
                          const std::string& fast_advertisement_service_uuid) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start BLE scanning with empty service id.";
    return false;
  }

  if (IsScanningLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Refusing to start scan of BLE peripherals because "
                         "another scanning is already in-progress.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't start BLE scanning because Bluetooth was never turned on";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't scan BLE peripherals because BLE isn't available.";
    return false;
  }

  discovered_peripheral_tracker_->StartTracking(
      service_id, std::move(callback), fast_advertisement_service_uuid);

  std::vector<std::string> service_uuids{std::string(kCopresenceServiceUuid)};
  if (!medium_.StartScanning(
          service_uuids, PowerLevelToPowerMode(power_level),
          {
              .advertisement_found_cb =
                  [this](const BleAdvertisementData& advertisement_data) {
                    RunOnBleThread([this, &advertisement_data]() {
                      discovered_peripheral_tracker_
                          ->ProcessFoundBleAdvertisement(
                              advertisement_data, GetAdvertisementFetcher());
                    });
                  },
          })) {
    NEARBY_LOGS(INFO) << "Failed to start scan of BLE services.";
    discovered_peripheral_tracker_->StopTracking(service_id);
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on BLE scanning with service id=" << service_id;
  // Mark the fact that we're currently performing a BLE scanning.
  scanning_info_.Add(service_id);
  return true;
}

bool BleV2::StopScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsScanningLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Can't turn off BLE sacanning because we never "
                         "started scanning.";
    return false;
  }

  // TODO(edwinwu): Cancel lost alarm

  NEARBY_LOG(INFO, "Turned off BLE scanning with service id=%s",
             service_id.c_str());
  bool is_stop = medium_.StopScanning();
  discovered_peripheral_tracker_->StopTracking(service_id);
  scanning_info_.Remove(service_id);
  return is_stop;
}

bool BleV2::IsScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsScanningLocked(service_id);
}

bool BleV2::IsAvailableLocked() const { return medium_.IsValid(); }

bool BleV2::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

bool BleV2::IsScanningLocked(const std::string& service_id) {
  return scanning_info_.Existed(service_id);
}

bool BleV2::IsAdvertisementGattServerRunningLocked() {
  return gatt_server_info_ != nullptr;
}

bool BleV2::StartAdvertisementGattServerLocked(
    const std::string& service_id, const ByteArray& gatt_advertisement) {
  if (IsAdvertisementGattServerRunningLocked()) {
    NEARBY_LOGS(INFO) << "Refusing to start an advertisement GATT server "
                         "because one is already running.";
    return false;
  }

  GattServer gatt_server = medium_.StartGattServer({
      .characteristic_subscription_cb =
          [](const ::nearby::cal::api::ServerGattConnection& connection,
             GattCharacteristic characteristic) {
            // Nothing else to do for now.
          },
      .characteristic_unsubscription_cb =
          [](const ::nearby::cal::api::ServerGattConnection& connection,
             GattCharacteristic characteristic) {
            // Nothing else to do for now.
          },
  });
  if (!gatt_server.IsValid()) {
    NEARBY_LOGS(INFO) << "Unable to start an advertisement GATT server.";
    return false;
  }

  if (!GenerateAdvertisementCharacteristic(
          /* slot= */ 0, gatt_advertisement, gatt_server)) {
    gatt_server.Stop();
    return false;
  }

  // Insert the advertisements into their open advertisement slots.
  gatt_advertisement_info_.Add(/* slot= */ 0,
                               std::make_pair(service_id, gatt_advertisement));

  gatt_server_info_ = absl::make_unique<GattServerInfo>();
  gatt_server_info_->gatt_server = std::move(gatt_server);
  return true;
}

bool BleV2::GenerateAdvertisementCharacteristic(
    int slot, const ByteArray& gatt_advertisement, GattServer& gatt_server) {
  std::vector<::nearby::cal::api::GattCharacteristic::Permission> permissions{
      ::nearby::cal::api::GattCharacteristic::Permission::kRead};
  std::vector<::nearby::cal::api::GattCharacteristic::Property> properties{
      ::nearby::cal::api::GattCharacteristic::Property::kRead};

  GattCharacteristic gatt_characteristic = gatt_server.CreateCharacteristic(
      std::string(kCopresenceServiceUuid),
      mediums::BleUtils::GenerateAdvertisementUuid(slot), permissions,
      properties);

  if (gatt_characteristic.IsValid()) {
    NEARBY_LOGS(INFO) << "Unable to create and add a characterstic to the gatt "
                         "server for the advertisement.";
    return false;
  }

  if (!gatt_server.UpdateCharacteristic(gatt_characteristic,
                                        gatt_advertisement)) {
    NEARBY_LOGS(INFO) << "Unable to write a value to the GATT characteristic.";
    return false;
  }

  return true;
}

std::unique_ptr<mediums::AdvertisementReadResult>
BleV2::ProcessFetchGattAdvertisementsRequest(
    BlePeripheralV2& peripheral, int num_slots,
    mediums::AdvertisementReadResult* advertisement_read_result) {
  MutexLock lock(&mutex_);

  std::unique_ptr<mediums::AdvertisementReadResult> arr =
      absl::make_unique<mediums::AdvertisementReadResult>();

  if (advertisement_read_result != nullptr) {
    arr.reset(advertisement_read_result);
  }

  if (!peripheral.IsValid()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "ble peripheral is null.";
    return arr;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "Bluetooth was never turned on.";
    return arr;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "BLE is not available.";
    return arr;
  }

  return InternalReadAdvertisementFromGattServerLocked(peripheral, num_slots,
                                                       std::move(arr));
}

std::unique_ptr<mediums::AdvertisementReadResult>
BleV2::InternalReadAdvertisementFromGattServerLocked(
    BlePeripheralV2& peripheral, int num_slots,
    std::unique_ptr<mediums::AdvertisementReadResult>
        advertisement_read_result) {
  // Attempt to connect and read some GATT characteristics.
  bool read_success = true;

  ClientGattConnection gatt_connection = medium_.ConnectToGattServer(
      peripheral, kDefaultMtu, PowerLevelToPowerMode(PowerLevel::kHighPower),
      {
          .disconnected_cb =
              [](ClientGattConnection& connection) {
                // Nothing else to do for now.
              },
      });
  if (gatt_connection.IsValid() && gatt_connection.DiscoverServices()) {
    // Read all advertisements from all slots that we haven't read from yet.
    for (int slot = 0; slot < num_slots; ++slot) {
      // Make sure we haven't already read this advertisement before.
      if (advertisement_read_result->HasAdvertisement(slot)) {
        continue;
      }

      // Make sure the characteristic even exists for this slot number. If
      // the characteristic doesn't exist, we shouldn't count the fetch as a
      // failure because there's nothing we could've done about a
      // non-existed characteristic.
      GattCharacteristic gatt_characteristic =
          gatt_connection.GetCharacteristic(
              std::string(kCopresenceServiceUuid),
              mediums::BleUtils::GenerateAdvertisementUuid(slot));
      if (!gatt_characteristic.IsValid()) {
        continue;
      }

      // Read advertisement data from the characteristic associated with this
      // slot.
      auto characteristic_byte =
          gatt_connection.ReadCharacteristic(gatt_characteristic);
      if (characteristic_byte.has_value()) {
        advertisement_read_result->AddAdvertisement(slot, *characteristic_byte);
        NEARBY_LOGS(VERBOSE)
            << "Successfully read advertisement at slot=" << slot
            << " on peripheral=" << &peripheral;
      } else {
        NEARBY_LOGS(WARNING) << "Can't read advertisement for slot=" << slot
                             << " on peripheral=" << &peripheral;
        read_success = false;
      }
      // Whether or not we succeeded with this slot, we should try reading the
      // other slots to get as many advertisements as possible before
      // returning a success or failure.
    }
    gatt_connection.Disconnect();
  } else {
    NEARBY_LOGS(WARNING)
        << "Can't connect to advertisement GATT server for peripheral="
        << &peripheral;
    read_success = false;
  }

  advertisement_read_result->RecordLastReadStatus(read_success);
  return advertisement_read_result;
}

bool BleV2::StopAdvertisementGattServerLocked() {
  if (!IsAdvertisementGattServerRunningLocked()) {
    NEARBY_LOGS(INFO) << "Unable to stop the advertisement GATT server because "
                         "it's not running.";
    return false;
  }

  if (gatt_server_info_) {
    gatt_server_info_->gatt_server.Stop();
    gatt_server_info_.release();
  }
  return true;
}

ByteArray BleV2::CreateAdvertisementHeader() {
  // Create a randomized dummy service id to anonymize the header with.
  ByteArray dummy_service_id_bytes =
      Utils::GenerateRandomBytes(kDummyServiceIdLength);
  std::string dummy_service_id{dummy_service_id_bytes};

  // Create a bloom filter with the dummy service id.
  mediums::BloomFilter<
      mediums::BleAdvertisementHeader::kServiceIdBloomFilterLength>
      bloom_filter;
  bloom_filter.Add(dummy_service_id);

  // Add the service id for each GATT advertisement into the bloom filter, while
  // also creating a hash of dummyServiceIdBytes + all GATT advertisements.
  ByteArray advertisement_hash =
      mediums::BleUtils::GenerateAdvertisementHash(dummy_service_id_bytes);
  int num_slots = 0;
  for (const auto& item : gatt_advertisement_info_.gatt_advertisements) {
    std::string service_id = item.second.first;
    ByteArray gatt_advertisment = item.second.second;
    bloom_filter.Add(service_id);

    // Compute the next hash by taking the hash of [previous hash] + [next
    // advertisement data].
    std::string advertisement_bodies = absl::StrCat(
        std::string(advertisement_hash), std::string(gatt_advertisment));

    advertisement_hash = mediums::BleUtils::GenerateAdvertisementHash(
        ByteArray{std::move(advertisement_bodies)});
    num_slots++;
  }

  return ByteArray(mediums::BleAdvertisementHeader(
      mediums::BleAdvertisementHeader::Version::kV2,
      /*extended_advertisement=*/false, num_slots, ByteArray(bloom_filter),
      advertisement_hash, /*psm=*/0));
}

ByteArray BleV2::UnwrapAdvertisementBytes(
    const ByteArray& medium_advertisement_data) {
  mediums::BleAdvertisement medium_ble_advertisement{medium_advertisement_data};
  if (!medium_ble_advertisement.IsValid()) {
    return ByteArray{};
  }

  return medium_ble_advertisement.GetData();
}

PowerMode BleV2::PowerLevelToPowerMode(PowerLevel power_level) {
  PowerMode power_mode = PowerMode::kUnknown;
  switch (power_level) {
    case PowerLevel::kHighPower:
      power_mode = PowerMode::kHigh;
      break;
    case PowerLevel::kLowPower:
      power_mode = PowerMode::kHigh;
  }
  return power_mode;
}

void BleV2::RunOnBleThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

mediums::DiscoveredPeripheralTracker::AdvertisementFetcher
BleV2::GetAdvertisementFetcher() {
  return {
      .fetch_advertisements =
          [this](BlePeripheralV2& peripheral, int num_slots,
                 mediums::AdvertisementReadResult* advertisement_read_result)
          -> std::unique_ptr<mediums::AdvertisementReadResult> {
        return ProcessFetchGattAdvertisementsRequest(peripheral, num_slots,
                                                     advertisement_read_result);
      },
  };
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
