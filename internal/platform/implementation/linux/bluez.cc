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

#include <sdbus-c++/Types.h>

#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/linux/bluez.h"

namespace nearby {
namespace linux {
namespace bluez {
std::string device_object_path(const sdbus::ObjectPath &adapter_object_path,
                               absl::string_view mac_address) {
  return absl::Substitute(
      "$0/dev_$1", adapter_object_path,
      absl::StrReplaceAll(absl::AsciiStrToUpper(mac_address), {{":", "_"}}));
}

sdbus::ObjectPath profile_object_path(absl::string_view service_uuid) {
  return absl::Substitute(
      "/com/google/nearby/medium/bluetooth_classic/profiles/$0",
      absl::StrReplaceAll(service_uuid, {{"-", "_"}}));
}

sdbus::ObjectPath adapter_object_path(absl::string_view name) {
  return absl::Substitute("/org/bluez/$0", name);
}

sdbus::ObjectPath gatt_service_path(size_t num) {
  return absl::Substitute("$0/service$1", NEARBY_BLE_GATT_PATH_ROOT, num);
}

sdbus::ObjectPath gatt_characteristic_path(
    const sdbus::ObjectPath &service_path, size_t num) {
  return absl::Substitute("$0/char$1", service_path, num);
}

sdbus::ObjectPath ble_advertisement_path(size_t num) {
  return absl::Substitute("/com/google/nearby/medium/ble/advertisement/$0",
                          num);
}

sdbus::ObjectPath advertisement_monitor_path(absl::string_view uuid) {
  return absl::Substitute(
      "/com/google/nearby/medium/ble/advertisement/monitor/$0",
      absl::StrReplaceAll(uuid, {{"-", "_"}}));
}

int16_t TxPowerLevelDbm(api::ble_v2::TxPowerLevel level) {
  switch (level) {
    case api::ble_v2::TxPowerLevel::kUnknown:
      return 0;
    case api::ble_v2::TxPowerLevel::kUltraLow:
      return -3;
    case api::ble_v2::TxPowerLevel::kLow:
      return 0;
    case api::ble_v2::TxPowerLevel::kMedium:
      return 3;
    case api::ble_v2::TxPowerLevel::kHigh:
      return 6;
  }
}

}  // namespace bluez
}  // namespace linux
}  // namespace nearby
