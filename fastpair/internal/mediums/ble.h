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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_BLE_BLE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_BLE_BLE_H_

#include <string>

#include "fastpair/internal/mediums/bluetooth_radio.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace fastpair {

// Provides the operations that can be performed on the Bluetooth Low Energy
// (BLE) medium.
class Ble {
 public:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;

  explicit Ble(BluetoothRadio& bluetooth_radio);
  Ble(Ble&&) = delete;
  Ble& operator=(Ble&&) = delete;
  ~Ble() = default;

  // Returns true, if Ble communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables Ble scanning mode. Will report any discoverable peripherals in
  // range through a callback. Returns true, if scanning mode was enabled,
  // false otherwise.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_pair_service_uuid,
                     DiscoveredPeripheralCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Ble discovery mode.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Return true if Ble is currenlty scanning.
  bool IsScanning() ABSL_LOCKS_EXCLUDED(mutex_);

  // Return BleMedium
  BleMedium& GetMedium() { return medium_; }

 private:
  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsDiscovering(), but must be called with mutex_ held.
  bool IsScanningLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  DiscoveredPeripheralCallback discovered_peripheral_callback_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  BleMedium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  bool is_scanning_ = false;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_BLE_BLE_H_
