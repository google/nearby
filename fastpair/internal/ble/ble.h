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

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace fastpair {

class Ble {
 public:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;

  Ble() = default;
  Ble(Ble&&) = default;
  Ble& operator=(Ble&&) = default;

  // Reverts the Ble to its original state.
  ~Ble();

  // Enables Bluetooth. Returns true if enabled successfully.
  // This must be called before attempting to invoke any other methods of
  // this class.
  bool Enable();

  // Disables Bluetooth. Returns true if disabled successfully.
  bool Disable();

  // Returns true if the Bluetooth radio is currently enabled.
  bool IsEnabled() const;

  // Returns true if Ble communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if this object owns a valid platform implementation.
  bool IsMediumValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsValid();
  }

  // Returns true if this object has a valid BluetoothAdapter reference.
  bool IsAdapterValid() const { return adapter_.IsValid(); }

  // Return true if Ble is currenlty scanning.
  bool IsScanning() ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables Ble scanning mode. Will report any discoverable peripherals in
  // range through a callback. Returns true, if scanning mode was enabled,
  // false otherwise.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_pair_service_uuid,
                     DiscoveredPeripheralCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables Ble discovery mode.
  bool StopScanning(const std::string& service_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Return BleMedium
  BleMedium& getMedium() { return medium_; }

  // Return BluetoothAdapter
  BluetoothAdapter& GetBluetoothAdapter() { return adapter_; }

 private:
  mutable Mutex mutex_;
  // BluetoothAdapter::IsValid() will return false if BT is not supported.
  BluetoothAdapter adapter_;
  DiscoveredPeripheralCallback discovered_peripheral_callback_;
  BleMedium medium_ ABSL_GUARDED_BY(mutex_){adapter_};
  bool is_scanning_ = false;

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsDiscovering(), but must be called with mutex_ held.
  bool IsScanningLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool SetBluetoothState(bool enable);

  bool IsInDesiredState(bool should_be_enabled) const;

  // To be called in enable() and disable(). This will remember the
  // original state of the ble before any ble state has been modified.
  // Returns false if Bluetooth doesn't exist on the device and the state cannot
  // be obtained.
  bool SaveOriginalState();

  // The Ble's original state, before we modified it. True if
  // originally enabled, false if originally disabled.
  // We restore the radio to its original state in the destructor.
  AtomicBoolean originally_enabled_{false};

  // false if we never modified the radio state, true otherwise.
  AtomicBoolean ever_saved_state_{false};
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_BLE_BLE_H_
