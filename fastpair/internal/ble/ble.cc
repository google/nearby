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

#include "fastpair/internal/ble/ble.h"

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

Ble::~Ble() {
  // We never enabled Bluetooth, nothing to do.
  if (!ever_saved_state_.Get()) {
    NEARBY_LOG(INFO, "BT adapter was not used. Not touching HW.");
    return;
  }

  NEARBY_LOG(INFO, "Bring BT adapter to original state");
  if (!SetBluetoothState(originally_enabled_.Get())) {
    NEARBY_LOG(INFO, "Failed to restore BT adapter original state.");
  }
}

bool Ble::Enable() {
  if (!SaveOriginalState()) {
    return false;
  }

  return SetBluetoothState(true);
}

bool Ble::Disable() {
  if (!SaveOriginalState()) {
    return false;
  }

  return SetBluetoothState(false);
}

bool Ble::IsEnabled() const {
  return IsAdapterValid() && IsInDesiredState(true);
}

bool Ble::SetBluetoothState(bool enable) {
  return adapter_.SetStatus(enable ? BluetoothAdapter::Status::kEnabled
                                   : BluetoothAdapter::Status::kDisabled);
}

bool Ble::IsInDesiredState(bool should_be_enabled) const {
  return adapter_.IsEnabled() == should_be_enabled;
}

bool Ble::SaveOriginalState() {
  if (!IsAdapterValid()) {
    return false;
  }

  // If we haven't saved the original state of the radio, save it.
  if (!ever_saved_state_.Set(true)) {
    originally_enabled_.Set(adapter_.IsEnabled());
  }

  return true;
}

bool Ble::IsAvailable() const {
  MutexLock lock(&mutex_);
  return IsAvailableLocked();
}

bool Ble::IsAvailableLocked() const {
  return medium_.IsValid() && adapter_.IsValid() && adapter_.IsEnabled();
}

bool Ble::StartScanning(const std::string& service_id,
                        const std::string& fast_pair_service_uuid,
                        DiscoveredPeripheralCallback callback) {
  MutexLock lock(&mutex_);

  discovered_peripheral_callback_ = std::move(callback);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start BLE scanning with empty service id.";
    return false;
  }

  if (IsScanningLocked()) {
    NEARBY_LOGS(INFO) << "Refusing to start BLE scanning because "
                         "another scanning is already in-progress.";
    return false;
  }

  if (!IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't start BLE scanning because Bluetooth was NOT enabled";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't scan BLE scanning because BLE isn't available.";
    return false;
  }

  if (!medium_.StartScanning(
          service_id, fast_pair_service_uuid,
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
                    discovered_peripheral_callback_.peripheral_discovered_cb(
                        peripheral, service_id, medium_advertisement_bytes,
                        fast_advertisement);
                  },
              .peripheral_lost_cb =
                  [this](BlePeripheral& peripheral,
                         const std::string& service_id) {
                    discovered_peripheral_callback_.peripheral_lost_cb(
                        peripheral, service_id);
                  },
          })) {
    NEARBY_LOGS(INFO) << "Failed to start BLE scanning";
    return false;
  }

  NEARBY_LOGS(INFO) << "Start BLE scanning with service id=" << service_id;
  // Mark the fact that we're currently performing a BLE scanning.
  is_scanning_ = true;
  return true;
}

bool Ble::StopScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);

  if (!IsScanningLocked()) {
    NEARBY_LOGS(INFO) << "Can't stop BLE scanning because we never "
                         "started scanning.";
    return false;
  }

  NEARBY_LOG(INFO, "Stop BLE scanning with service id=%s", service_id.c_str());
  bool ret = medium_.StopScanning(service_id);
  is_scanning_ = false;
  return ret;
}

bool Ble::IsScanning() {
  MutexLock lock(&mutex_);

  return IsScanningLocked();
}

bool Ble::IsScanningLocked() { return is_scanning_; }

}  // namespace fastpair
}  // namespace nearby
