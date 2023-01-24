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

#include "connections/implementation/mediums/ble.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/prng.h"

namespace nearby {
namespace connections {

ByteArray Ble::GenerateHash(const std::string& source, size_t size) {
  return Utils::Sha256Hash(source, size);
}

ByteArray Ble::GenerateDeviceToken() {
  return Utils::Sha256Hash(std::to_string(Prng().NextUint32()),
                           mediums::BleAdvertisement::kDeviceTokenLength);
}

Ble::Ble(BluetoothRadio& radio) : radio_(radio) {}

bool Ble::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool Ble::IsAvailableLocked() const {
  return medium_.IsValid() && adapter_.IsValid() && adapter_.IsEnabled();
}

bool Ble::StartAdvertising(const std::string& service_id,
                           const ByteArray& advertisement_bytes,
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

  NEARBY_LOGS(INFO) << "Turning on BLE advertising (advertisement size="
                    << advertisement_bytes.size() << ")"
                    << ", service id=" << service_id
                    << ", fast advertisement service uuid="
                    << fast_advertisement_service_uuid;

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

  if (!medium_.StartAdvertising(service_id, medium_advertisement_bytes,
                                fast_advertisement_service_uuid)) {
    NEARBY_LOGS(ERROR)
        << "Failed to turn on BLE advertising with advertisement bytes="
        << absl::BytesToHexString(advertisement_bytes.data())
        << ", size=" << advertisement_bytes.size()
        << ", fast advertisement service uuid="
        << fast_advertisement_service_uuid;
    return false;
  }

  advertising_info_.Add(service_id);
  return true;
}

bool Ble::StopAdvertising(const std::string& service_id) {
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

bool Ble::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool Ble::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

bool Ble::StartScanning(const std::string& service_id,
                        const std::string& fast_advertisement_service_uuid,
                        DiscoveredPeripheralCallback callback) {
  MutexLock lock(&mutex_);

  discovered_peripheral_callback_ = std::move(callback);

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

  if (!medium_.StartScanning(
          service_id, fast_advertisement_service_uuid,
          {
              .peripheral_discovered_cb =
                  [this](BlePeripheral& peripheral,
                         const std::string& service_id,
                         const ByteArray& medium_advertisement_bytes,
                         bool fast_advertisement) {
                    // Don't bother trying to parse zero byte advertisements.
                    if (medium_advertisement_bytes.size() == 0) {
                      NEARBY_LOGS(INFO) << "Skipping zero byte advertisement "
                                        << "with service_id: " << service_id;
                      return;
                    }
                    // Unwrap connection BleAdvertisement from medium
                    // BleAdvertisement.
                    auto connection_advertisement_bytes =
                        UnwrapAdvertisementBytes(medium_advertisement_bytes);
                    discovered_peripheral_callback_.peripheral_discovered_cb(
                        peripheral, service_id, connection_advertisement_bytes,
                        fast_advertisement);
                  },
              .peripheral_lost_cb =
                  [this](BlePeripheral& peripheral,
                         const std::string& service_id) {
                    discovered_peripheral_callback_.peripheral_lost_cb(
                        peripheral, service_id);
                  },
          })) {
    NEARBY_LOGS(INFO) << "Failed to start scan of BLE services.";
    return false;
  }

  NEARBY_LOGS(INFO) << "Turned on BLE scanning with service id=" << service_id;
  // Mark the fact that we're currently performing a BLE discovering.
  scanning_info_.Add(service_id);
  return true;
}

bool Ble::StopScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsScanningLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Can't turn off BLE scanning because we never "
                         "started scanning.";
    return false;
  }

  NEARBY_LOG(INFO, "Turned off BLE scanning with service id=%s",
             service_id.c_str());
  bool ret = medium_.StopScanning(service_id);
  scanning_info_.Clear();
  return ret;
}

bool Ble::IsScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsScanningLocked(service_id);
}

bool Ble::IsScanningLocked(const std::string& service_id) {
  return scanning_info_.Existed(service_id);
}

bool Ble::StartAcceptingConnections(const std::string& service_id,
                                    AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BLE connections with empty service id.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BLE connections for " << service_id
        << " because another BLE peripheral socket is already in-progress.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't start accepting BLE connections for "
                      << service_id << " because Bluetooth isn't enabled.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't start accepting BLE connections for "
                      << service_id << " because BLE isn't available.";
    return false;
  }

  if (!medium_.StartAcceptingConnections(service_id, std::move(callback))) {
    NEARBY_LOGS(INFO) << "Failed to accept connections callback for "
                      << service_id << " .";
    return false;
  }

  accepting_connections_info_.Add(service_id);
  return true;
}

bool Ble::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Can't stop accepting BLE connections because it was never started.";
    return false;
  }

  bool ret = medium_.StopAcceptingConnections(service_id);
  // Reset our bundle of accepting connections state to mark that we're no
  // longer accepting connections.
  accepting_connections_info_.Remove(service_id);
  return ret;
}

bool Ble::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  return IsAcceptingConnectionsLocked(service_id);
}

bool Ble::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return accepting_connections_info_.Existed(service_id);
}

BleSocket Ble::Connect(BlePeripheral& peripheral, const std::string& service_id,
                       CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "BLE::Connect: service=" << &peripheral;
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  BleSocket socket;

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to create BLE socket with empty service_id.";
    return socket;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't create client BLE socket to " << &peripheral
                      << " because Bluetooth isn't enabled.";
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client BLE socket [service_id="
                      << service_id << "]; BLE isn't available.";
    return socket;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Can't create client BLE socket due to cancel.";
    return socket;
  }

  socket = medium_.Connect(peripheral, service_id, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via BLE [service=" << service_id
                      << "]";
  }

  return socket;
}

ByteArray Ble::UnwrapAdvertisementBytes(
    const ByteArray& medium_advertisement_data) {
  mediums::BleAdvertisement medium_ble_advertisement{medium_advertisement_data};
  if (!medium_ble_advertisement.IsValid()) {
    return ByteArray{};
  }

  return medium_ble_advertisement.GetData();
}

}  // namespace connections
}  // namespace nearby
