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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_

#include "internal/platform/atomic_boolean.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace fastpair {

// Provides the operations that can be performed on the Bluetooth radio.
class BluetoothRadio {
 public:
  BluetoothRadio();
  BluetoothRadio(BluetoothRadio&&) = default;
  BluetoothRadio& operator=(BluetoothRadio&&) = default;

  // Reverts the Bluetooth radio to its original state.
  ~BluetoothRadio();

  // Enables Bluetooth.
  //
  // This must be called before attempting to invoke any other methods of
  // this class.
  //
  // Returns true if enabled successfully.
  bool Enable();

  // Disables Bluetooth.
  //
  // Returns true if disabled successfully.
  bool Disable();

  // Returns true if the Bluetooth radio is currently enabled.
  bool IsEnabled() const;

  // Returns result of BluetoothAdapter::IsValid() for private adapter instance.
  bool IsAdapterValid() const { return bluetooth_adapter_.IsValid(); }

  BluetoothAdapter& GetBluetoothAdapter() { return bluetooth_adapter_; }

 private:
  bool SetBluetoothState(bool enable);
  bool IsInDesiredState(bool should_be_enabled) const;
  // To be called in enable() and disable(). This will remember the
  // original state of the radio before any radio state has been modified.
  // Returns false if Bluetooth doesn't exist on the device and the state cannot
  // be obtained.
  bool SaveOriginalState();

  // BluetoothAdapter::IsValid() will return false if BT is not supported.
  BluetoothAdapter bluetooth_adapter_;

  // The Bluetooth radio's original state, before we modified it. True if
  // originally enabled, false if originally disabled.
  // We restore the radio to its original state in the destructor.

  AtomicBoolean originally_enabled_{false};
  // false if we never modified the radio state, true otherwise.
  AtomicBoolean ever_saved_state_{false};
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_
