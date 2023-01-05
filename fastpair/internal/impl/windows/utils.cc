// Copyright 2022 Google LLC
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

#include "fastpair/internal/impl/windows/utils.h"

// Standard C/C++ headers
#include <string>

// Third party headers
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace windows {

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress) {
  std::string buffer = absl::StrFormat(
      "%2llx:%2llx:%2llx:%2llx:%2llx:%2llx", bluetoothAddress >> 40,
      (bluetoothAddress >> 32) & 0xff, (bluetoothAddress >> 24) & 0xff,
      (bluetoothAddress >> 16) & 0xff, (bluetoothAddress >> 8) & 0xff,
      bluetoothAddress & 0xff);

  return absl::AsciiStrToUpper(buffer);
}

uint64_t mac_address_string_to_uint64(absl::string_view mac_address) {
  nearby::ByteArray mac_address_array =
      nearby::BluetoothUtils::FromString(mac_address);
  uint64_t mac_address_uint64 = 0;
  for (int i = 0; i < mac_address_array.size(); i++) {
    mac_address_uint64 <<= 8;
    mac_address_uint64 |= static_cast<uint8_t>(
        static_cast<unsigned char>(*(mac_address_array.data() + i)));
  }
  return mac_address_uint64;
}

}  // namespace windows
}  // namespace nearby
