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

#include "core/internal/mediums/ble_v2.h"

#include <string>
#include <utility>

#include "core/internal/mediums/ble_v2/ble_advertisement.h"
#include "core/internal/mediums/utils.h"
#include "platform/base/prng.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

ByteArray BleV2::GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray BleV2::GenerateDeviceToken() {
  return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                           mediums::BleAdvertisement::kDeviceTokenLength);
}

BleV2::BleV2(BluetoothRadio& radio) : radio_(radio) {}

bool BleV2::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool BleV2::IsAvailableLocked() const { return medium_.IsValid(); }

bool BleV2::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
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
  ByteArray service_id_hash{GenerateHash(
      service_id, mediums::BleAdvertisement::kServiceIdHashLength)};
  ByteArray medium_advertisement_bytes{mediums::BleAdvertisement{
      mediums::BleAdvertisement::Version::kV2,
      mediums::BleAdvertisement::SocketVersion::kV2,
      fast_advertisement ? ByteArray{} : service_id_hash, advertisement_bytes,
      GenerateDeviceToken()}};
  if (medium_advertisement_bytes.Empty()) {
    NEARBY_LOGS(INFO) << "Failed to BLE advertise because we could not "
                         "create a medium advertisement.";
    return false;
  }

  // TODO(edwinwu): Implement StartAdvertisementGattServer() and create
  // AdvertisementHeader

  // TODO(edwinwu): Assemble AdvertisingData and ScanResponseData.

  // TODO(edwinwu): StartAdvertising.
  // medium_->StartAdvertising(advertising_data, scan_response_data);

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
  bool ret = medium_.StopAdvertising(service_id);
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising.
  advertising_info_.Remove(service_id);
  return ret;
}

bool BleV2::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool BleV2::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
