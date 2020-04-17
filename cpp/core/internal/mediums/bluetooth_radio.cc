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

#include "core/internal/mediums/bluetooth_radio.h"

#include "platform/exception.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
std::int64_t BluetoothRadio<Platform>::kPauseBetweenToggleDurationMillis = 3000;

template <typename Platform>
BluetoothRadio<Platform>::BluetoothRadio()
    : bluetooth_adapter_(Platform::createBluetoothAdapter()),
      thread_utils_(Platform::createThreadUtils()),
      originally_enabled_() {
  if (bluetooth_adapter_.isNull()) {
    // TODO(reznor): log.atSevere().log("Failed to retrieve default
    // BluetoothAdapter, Bluetooth is unsupported.");
  }
}

template <typename Platform>
BluetoothRadio<Platform>::~BluetoothRadio() {
  // We never enabled Bluetooth, nothing to do.
  if (originally_enabled_.isNull()) {
    return;
  }

  // Make sure we cleanup the one non-ScopedPtr member before we leave the
  // destructor.
  ScopedPtr<Ptr<AtomicBoolean> > scoped_originally_enabled(originally_enabled_);

  // Toggle Bluetooth regardless of our original state. Some devices/chips can
  // start to freak out after some time (e.g. b/37775337), and this helps to
  // ensure BT resets properly.
  toggle();

  if (!setBluetoothState(originally_enabled_->get())) {
    // TODO(reznor): log.atWarning().log("Failed to turn Bluetooth back to its
    // original state.");
  }
}

template <typename Platform>
bool BluetoothRadio<Platform>::enable() {
  if (!saveOriginalState()) {
    return false;
  }

  return setBluetoothState(true);
}

template <typename Platform>
bool BluetoothRadio<Platform>::disable() {
  if (!saveOriginalState()) {
    return false;
  }

  return setBluetoothState(false);
}

template <typename Platform>
bool BluetoothRadio<Platform>::isEnabled() {
  return !bluetooth_adapter_.isNull() && isInDesiredState(true);
}

template <typename Platform>
void BluetoothRadio<Platform>::toggle() {
  if (!saveOriginalState()) {
    return;
  }

  if (!setBluetoothState(false)) {
    // TODO(reznor): log.atWarning().log("Failed to turn Bluetooth off while
    // toggling state.");
  }

  if (Exception::INTERRUPTED ==
      thread_utils_->sleep(kPauseBetweenToggleDurationMillis)) {
    // TODO(reznor): log.atSevere().withCause(e).log("Interrupted while waiting
    // in between a Bluetooth toggle.");
    return;
  }

  if (!setBluetoothState(true)) {
    // TODO(reznor): log.atWarning().log("Failed to turn Bluetooth on while
    // toggling state.");
  }
}

template <typename Platform>
bool BluetoothRadio<Platform>::setBluetoothState(bool enable) {
  return bluetooth_adapter_->setStatus(
      enable ? BluetoothAdapter::Status::ENABLED
             : BluetoothAdapter::Status::DISABLED);
}

template <typename Platform>
bool BluetoothRadio<Platform>::isInDesiredState(bool should_be_enabled) const {
  return ((should_be_enabled && bluetooth_adapter_->isEnabled()) ||
          (!should_be_enabled && !bluetooth_adapter_->isEnabled()));
}

template <typename Platform>
bool BluetoothRadio<Platform>::saveOriginalState() {
  if (bluetooth_adapter_.isNull()) {
    return false;
  }

  // If we haven't saved the original state of the radio, save it.
  if (originally_enabled_.isNull()) {
    originally_enabled_ =
        Platform::createAtomicBoolean(bluetooth_adapter_->isEnabled());
  }

  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
