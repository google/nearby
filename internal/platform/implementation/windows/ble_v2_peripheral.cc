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

#include "internal/platform/implementation/windows/ble_v2_peripheral.h"
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace windows {

BleV2Peripheral::BleV2Peripheral(absl::string_view address) {
  if (!MacAddress::FromString(address, mac_address_)) {
    LOG(WARNING) << "Create BleV2Peripheral with invalid MAC: " << address;
  }
}

std::string BleV2Peripheral::GetAddress() const {
  if (mac_address_.IsSet()) {
    return mac_address_.ToString();
  }
  return "";
}

}  // namespace windows
}  // namespace nearby
