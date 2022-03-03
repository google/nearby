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

#include <string>
#include <string_view>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/prng.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::PowerMode;

constexpr int kMaxAdvertisementLength = 512;
constexpr int kDummyServiceIdLength = 128;
constexpr absl::string_view kCopresenceServiceUuid =
    "0000FEF3-0000-1000-8000-00805F9B34FB";

ByteArray GenerateAdvertisementHash(const ByteArray& advertisement_bytes) {
  return Utils::Sha256Hash(
      advertisement_bytes,
      mediums::BleAdvertisementHeader::kAdvertisementHashLength);
}

ByteArray GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray GenerateDeviceToken() {
  return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                           mediums::BleAdvertisement::kDeviceTokenLength);
}

}  // namespace

BleV2::BleV2(BluetoothRadio& radio)
    : radio_(radio), adapter_(radio_.GetBluetoothAdapter()) {}

bool BleV2::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

// TODO(edwinwu): Break down the function.
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
  const bool is_fast_advertisement = !fast_advertisement_service_uuid.empty();
  ByteArray service_id_hash{GenerateHash(
      service_id, mediums::BleAdvertisement::kServiceIdHashLength)};
  ByteArray medium_advertisement_bytes{mediums::BleAdvertisement{
      mediums::BleAdvertisement::Version::kV2,
      mediums::BleAdvertisement::SocketVersion::kV2,
      /* service_id_hash= */
      is_fast_advertisement ? ByteArray{} : service_id_hash,
      advertisement_bytes, GenerateDeviceToken()}};
  if (medium_advertisement_bytes.Empty()) {
    NEARBY_LOGS(INFO) << "Failed to BLE advertise because we could not wrap a "
                         "connection advertisement to medium advertisement.";
    return false;
  }

  // Stop any existing advertisement GATT servers. We don't stop it in
  // StopAdvertising() to avoid GATT issues with BLE sockets.
  if (IsAdvertisementGattServerRunning()) {
    StopAdvertisementGattServer();
  }

  // Assemble AdvertisingData and ScanResponseData.
  BleAdvertisementData advertising_data;
  BleAdvertisementData scan_response_data;
  if (is_fast_advertisement) {
    advertising_data.service_data.insert(
        {fast_advertisement_service_uuid, medium_advertisement_bytes});
    scan_response_data.service_uuids.insert(fast_advertisement_service_uuid);
  } else {
    // Start a GATT server to deliver the full advertisement data. If fail to
    // advertise the header, we must shut this down before the method returns.
    if (!StartAdvertisementGattServer(service_id, medium_advertisement_bytes)) {
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
      StopAdvertisementGattServer();
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

  // Convert power_mode from power_level.
  PowerMode power_mode = PowerMode::kUnknown;
  switch (power_level) {
    case PowerLevel::kHighPower:
      power_mode = PowerMode::kHigh;
      break;
    case PowerLevel::kLowPower:
      // Medium power is about the size of a conference room. Any lower power
      // it won't be visible at a distance.
      power_mode = PowerMode::kMedium;
      break;
    default:
      power_mode = PowerMode::kUnknown;
  }

  if (!medium_.StartAdvertising(advertising_data, scan_response_data,
                                power_mode)) {
    NEARBY_LOGS(ERROR)
        << "Failed to turn on BLE advertising with advertisement bytes="
        << absl::BytesToHexString(advertisement_bytes.data())
        << ", size=" << advertisement_bytes.size()
        << ", fast advertisement service uuid="
        << fast_advertisement_service_uuid;

    // If BLE advertising was not successful, stop the advertisement GATT
    // server.
    StopAdvertisementGattServer();
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

bool BleV2::IsAdvertising(const std::string& service_id) const {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool BleV2::IsAvailableLocked() const { return medium_.IsValid(); }

bool BleV2::IsAdvertisingLocked(const std::string& service_id) const {
  return advertising_info_.Existed(service_id);
}

// TODO(b/213835576): Implement GattServer
bool BleV2::IsAdvertisementGattServerRunning() { return false; }

bool BleV2::StartAdvertisementGattServer(const std::string& service_id,
                                         const ByteArray& advertisement) {
  return false;
}

bool BleV2::StopAdvertisementGattServer() { return false; }

ByteArray BleV2::CreateAdvertisementHeader() {
  // Create a randomized dummy service id to anonymize the header with.
  ByteArray dummy_service_id_bytes =
      Utils::GenerateRandomBytes(kDummyServiceIdLength);
  std::string dummy_service_id{dummy_service_id_bytes};

  mediums::BloomFilter<
      mediums::BleAdvertisementHeader::kServiceIdBloomFilterLength>
      bloom_filter;
  bloom_filter.Add(dummy_service_id);

  ByteArray advertisement_hash =
      GenerateAdvertisementHash(dummy_service_id_bytes);
  for (const auto& item : gatt_advertisements_) {
    const std::string& service_id = item.second.first;
    const ByteArray& gatt_advertisement = item.second.second;
    bloom_filter.Add(service_id);

    // Compute the next hash according to the algorithm in
    // https://source.corp.google.com/piper///depot/google3/java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums/bluetooth/BluetoothLowEnergy.java;rcl=428397891;l=1043
    std::string advertisement_bodies = absl::StrCat(
        advertisement_hash.AsStringView(), gatt_advertisement.AsStringView());

    advertisement_hash =
        GenerateAdvertisementHash(ByteArray(std::move(advertisement_bodies)));
  }

  return ByteArray(mediums::BleAdvertisementHeader(
      mediums::BleAdvertisementHeader::Version::kV2,
      /*extended_advertisement=*/false,
      /*num_slots=*/gatt_advertisements_.size(), ByteArray(bloom_filter),
      advertisement_hash, /*psm=*/0));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
