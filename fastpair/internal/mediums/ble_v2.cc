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

#include "fastpair/internal/mediums/ble_v2.h"

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace fastpair {

BleV2::BleV2(BluetoothRadio& radio) : radio_(radio) {}

bool BleV2::IsAvailable() const {
  MutexLock lock(&mutex_);
  return IsAvailableLocked();
}

bool BleV2::IsAvailableLocked() const {
  return medium_.IsValid() && adapter_.IsValid() && adapter_.IsEnabled();
}

std::unique_ptr<RobustGattClient> BleV2::ConnectToGattServer(
    absl::string_view ble_address, RobustGattClient::ConnectionParams params,
    RobustGattClient::ConnectionStatusCallback connection_status_callback) {
  MutexLock lock(&mutex_);
  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't connect to GattServer because Bluetooth was NOT enabled";
    return nullptr;
  }
  if (!IsAvailableLocked()) {
    NEARBY_LOGS(VERBOSE)
        << __func__
        << "Can't connect to GattServer because BleV2 isn't available.";
    return nullptr;
  }

  BleV2Peripheral v2_peripheral =
      medium_.GetRemotePeripheral(std::string(ble_address));
  return std::make_unique<RobustGattClient>(
      medium_, v2_peripheral, std::move(params),
      std::move(connection_status_callback));
}
}  // namespace fastpair
}  // namespace nearby
