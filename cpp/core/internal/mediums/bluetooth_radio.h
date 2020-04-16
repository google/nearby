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

#ifndef CORE_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_
#define CORE_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_

#include <cstdint>

#include "platform/api/atomic_boolean.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/thread_utils.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Provides the operations that can be performed on the Bluetooth radio.
template <typename Platform>
class BluetoothRadio {
 public:
  BluetoothRadio();
  // Reverts the Bluetooth radio to its original state.
  ~BluetoothRadio();

  // Enables Bluetooth.
  //
  // This must be called before attempting to invoke any other methods of
  // this class.
  //
  // Returns true if enabled successfully.
  bool enable();
  // Disables Bluetooth.
  //
  // Returns true if disabled successfully.
  bool disable();
  // Returns true if the Bluetooth radio is currently enabled.
  bool isEnabled();

  void toggle();

 private:
  static std::int64_t kPauseBetweenToggleDurationMillis;

  bool setBluetoothState(bool enable);
  bool isInDesiredState(bool should_be_enabled) const;
  // To be called in enable(), disable(), and toggle(). This will remember the
  // original state of the radio before any radio state has been modified.
  // Returns false if Bluetooth doesn't exist on the device and the state cannot
  // be obtained.
  bool saveOriginalState();

  // Null if the device does not support Bluetooth.
  ScopedPtr<Ptr<BluetoothAdapter>> bluetooth_adapter_;
  ScopedPtr<Ptr<ThreadUtils>> thread_utils_;
  // The Bluetooth radio's original state, before we modified it. True if
  // originally enabled, false if originally disabled, null if we never modified
  // the radio state. We restore the radio to its original state in the
  // destructor.
  //
  // This is a Ptr instead of a ScopedPtr because it's lazily initialized
  // (and ScopedPtr doesn't support re-assignment).
  Ptr<AtomicBoolean> originally_enabled_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/bluetooth_radio.cc"

#endif  // CORE_INTERNAL_MEDIUMS_BLUETOOTH_RADIO_H_
