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

namespace nearby {
namespace windows {
namespace {
constexpr int kMacAddressLength = 17;
}

bool BleV2Peripheral::SetAddress(absl::string_view address) {
  // The address must be in format "00:B0:D0:63:C2:26".
  if (address.size() != kMacAddressLength) {
    NEARBY_LOGS(ERROR) << ": Invalid MAC address length.";
    return false;
  }

  for (int i = 0; i < kMacAddressLength; ++i) {
    if ((i % 3) == 0 || (i % 3) == 1) {
      if ((address[i] >= '0' && address[i] <= '9') ||
          (address[i] >= 'a' && address[i] <= 'f') ||
          (address[i] >= 'A' && address[i] <= 'F')) {
        continue;
      }
    } else {
      if (address[i] == ':') {
        continue;
      }
    }

    NEARBY_LOGS(ERROR) << ": Invalid MAC address format.";
    return false;
  }

  address_ = std::string(address);
  return true;
}

}  // namespace windows
}  // namespace nearby
