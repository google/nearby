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

#include "core_v2/internal/mediums/bluetooth_radio.h"

#include "platform_v2/base/exception.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/system_clock.h"

namespace location {
namespace nearby {
namespace connections {

constexpr absl::Duration BluetoothRadio::kPauseBetweenToggle;

BluetoothRadio::BluetoothRadio() {
  if (!IsAdapterValid()) {
    NEARBY_LOG(ERROR, "Bluetooth adapter is not valid: BT is not supported");
  }
}

BluetoothRadio::~BluetoothRadio() {
  // We never enabled Bluetooth, nothing to do.
  if (!ever_saved_state_.Get()) {
    NEARBY_LOG(INFO, "BT adapter was not used. Not touching HW.");
    return;
  }

  // Toggle Bluetooth regardless of our original state. Some devices/chips can
  // start to freak out after some time (e.g. b/37775337), and this helps to
  // ensure BT resets properly.
  NEARBY_LOG(INFO, "Toggle BT adapter state before releasing adapter.");
  Toggle();

  NEARBY_LOG(INFO, "Bring BT adapter to original state");
  if (!SetBluetoothState(originally_enabled_.Get())) {
    NEARBY_LOG(INFO, "Failed to restore BT adapter original state.");
  }
}

bool BluetoothRadio::Enable() {
  if (!SaveOriginalState()) {
    return false;
  }

  return SetBluetoothState(true);
}

bool BluetoothRadio::Disable() {
  if (!SaveOriginalState()) {
    return false;
  }

  return SetBluetoothState(false);
}

bool BluetoothRadio::IsEnabled() const {
  return IsAdapterValid() && IsInDesiredState(true);
}

bool BluetoothRadio::Toggle() {
  if (!SaveOriginalState()) {
    return false;
  }

  if (!SetBluetoothState(false)) {
    NEARBY_LOG(INFO, "BT Toggle: Failed to turn BT off.");
    return false;
  }

  if (SystemClock::Sleep(kPauseBetweenToggle).Raised(Exception::kInterrupted)) {
    NEARBY_LOG(INFO, "BT Toggle: interrupted before turing on.");
    return false;
  }

  if (!SetBluetoothState(true)) {
    NEARBY_LOG(INFO, "BT Toggle: Failed to turn BT on.");
    return false;
  }

  return true;
}

bool BluetoothRadio::SetBluetoothState(bool enable) {
  return bluetooth_adapter_.SetStatus(
      enable ? BluetoothAdapter::Status::kEnabled
             : BluetoothAdapter::Status::kDisabled);
}

bool BluetoothRadio::IsInDesiredState(bool should_be_enabled) const {
  return bluetooth_adapter_.IsEnabled() == should_be_enabled;
}

bool BluetoothRadio::SaveOriginalState() {
  if (!IsAdapterValid()) {
    return false;
  }

  // If we haven't saved the original state of the radio, save it.
  if (!ever_saved_state_.Set(true)) {
    originally_enabled_.Set(bluetooth_adapter_.IsEnabled());
  }

  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
