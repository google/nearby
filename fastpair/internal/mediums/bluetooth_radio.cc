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

#include "fastpair/internal/mediums/bluetooth_radio.h"

#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

BluetoothRadio::BluetoothRadio() {
  if (!IsAdapterValid()) {
    NEARBY_LOGS(ERROR) << "Bluetooth adapter is not valid: BT is not supported";
  }
}

BluetoothRadio::~BluetoothRadio() {
  NEARBY_LOGS(VERBOSE) << "~BluetoothRadio start";
  // We never enabled Bluetooth, nothing to do.
  if (!ever_saved_state_.Get()) {
    NEARBY_LOGS(INFO) << "BT adapter was not used. Not touching HW.";
    return;
  }
  NEARBY_LOGS(INFO) << "Bring BT adapter to original state";
  if (!SetBluetoothState(originally_enabled_.Get())) {
    NEARBY_LOGS(INFO) << "Failed to restore BT adapter original state.";
  }
  NEARBY_LOGS(VERBOSE) << "~BluetoothRadio done";
}

bool BluetoothRadio::Enable() {
  NEARBY_LOGS(INFO) << __func__;
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

}  // namespace fastpair
}  // namespace nearby
