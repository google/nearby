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

#include "internal/platform/connection_info.h"

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/bluetooth_connection_info.h"
#include "internal/platform/wifi_lan_connection_info.h"

namespace nearby {
ConnectionInfoVariant ConnectionInfo::FromDataElementBytes(
    absl::string_view data_element_bytes) {
  uint8_t type = data_element_bytes[2];
  if (type == kBluetoothMediumType) {
    auto result =
        BluetoothConnectionInfo::FromDataElementBytes(data_element_bytes);
    if (result.ok()) {
      return result.value();
    }
  } else if (type == kBleGattMediumType) {
    auto result = BleConnectionInfo::FromDataElementBytes(data_element_bytes);
    if (result.ok()) {
      return result.value();
    }
  } else if (type == kWifiLanMediumType) {
    auto result =
        WifiLanConnectionInfo::FromDataElementBytes(data_element_bytes);
    if (result.ok()) {
      return result.value();
    }
  }
  return absl::monostate();
}
}  // namespace nearby
