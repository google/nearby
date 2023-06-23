// Copyright 2023 Google LLC
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

#include "fastpair/internal/mediums/bluetooth_classic.h"

#include <memory>
#include <string>

#include "absl/strings/escaping.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace fastpair {

BluetoothClassic::BluetoothClassic(BluetoothRadio& radio) : radio_(radio) {}

bool BluetoothClassic::IsAvailable() const {
  MutexLock lock(&mutex_);
  return IsAvailableLocked();
}

bool BluetoothClassic::IsAvailableLocked() const {
  return medium_.IsValid() && adapter_.IsValid() && adapter_.IsEnabled();
}

void BluetoothClassic::AddObserver(BluetoothClassicMedium::Observer* observer) {
  MutexLock lock(&mutex_);
  medium_.AddObserver(observer);
}

void BluetoothClassic::RemoveObserver(
    BluetoothClassicMedium::Observer* observer) {
  MutexLock lock(&mutex_);
  medium_.RemoveObserver(observer);
}

bool BluetoothClassic::StartDiscovery() {
  MutexLock lock(&mutex_);
  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't discover BT devices because BT isn't enabled.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't discover BT devices because BT isn't available.";
    return false;
  }

  if (!medium_.StartDiscovery({})) {
    NEARBY_LOGS(INFO) << "Failed to start discovery of BT devices.";
    return false;
  }
  // Mark the fact that we're currently performing a Bluetooth scan.
  is_scanning = true;
  return true;
}

bool BluetoothClassic::StopDiscovery() {
  MutexLock lock(&mutex_);
  if (!IsDiscovering()) {
    NEARBY_LOGS(INFO)
        << "Can't stop discovery of BT devices because it never started.";
    return false;
  }

  if (!medium_.StopDiscovery()) {
    NEARBY_LOGS(INFO) << "Failed to stop discovery of Bluetooth devices.";
    return false;
  }
  is_scanning = false;
  return true;
}

bool BluetoothClassic::IsDiscovering() const { return is_scanning; }

std::unique_ptr<BluetoothPairing> BluetoothClassic::CreatePairing(
    absl::string_view public_address) {
  MutexLock lock(&mutex_);
  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << __func__
        << "Can't create bluetooth pairing because Bluetooth was NOT enabled";
    return nullptr;
  }
  if (!IsAvailableLocked()) {
    NEARBY_LOGS(VERBOSE) << __func__
                         << "Can't create bluetooth pairing because "
                            "BluetoothClassic isn't available.";
    return nullptr;
  }
  BluetoothDevice device = medium_.GetRemoteDevice(std::string(public_address));
  if (!device.IsValid()) {
    NEARBY_LOGS(INFO) << __func__
                      << "Can't create bluetooth pairing because Bluetooth "
                         "device was NOT found";
    return nullptr;
  }
  return medium_.CreatePairing(device);
}

BluetoothClassicMedium& BluetoothClassic::GetMedium() { return medium_; }
}  // namespace fastpair
}  // namespace nearby
