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

#include "connections/implementation/mediums/bluetooth_radio.h"

#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

BluetoothRadio::BluetoothRadio() {
  if (!IsAdapterValid()) {
    LOG(ERROR) << "Bluetooth adapter is not valid: BT is not supported";
  }
}

BluetoothRadio::~BluetoothRadio() {
  // We never enabled Bluetooth, nothing to do.
  if (!ever_saved_state_.Get()) {
    LOG(INFO) << "BT adapter was not used. Not touching HW.";
    return;
  }

  LOG(INFO) << "Bring BT adapter to original state";
  if (!SetBluetoothState(originally_enabled_.Get())) {
    LOG(INFO) << "Failed to restore BT adapter original state.";
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
