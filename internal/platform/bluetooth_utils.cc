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

#include "internal/platform/bluetooth_utils.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mac_address.h"

namespace nearby {

std::string BluetoothUtils::ToString(const ByteArray& bluetooth_mac_address) {
  if (bluetooth_mac_address.size() != kBluetoothMacAddressLength) {
    return "";
  }
  MacAddress mac_address;
  if (!MacAddress::FromBytes(
          absl::MakeConstSpan(
              reinterpret_cast<const uint8_t*>(bluetooth_mac_address.data()),
              bluetooth_mac_address.size()),
          mac_address) ||
      !mac_address.IsSet()) {
    return "";
  }
  return mac_address.ToString();
}

ByteArray BluetoothUtils::FromString(absl::string_view bluetooth_mac_address) {
  MacAddress mac_address;
  if (!MacAddress::FromString(bluetooth_mac_address, mac_address) ||
      !mac_address.IsSet()) {
    return ByteArray();
  }
  ByteArray address_bytes(BluetoothUtils::kBluetoothMacAddressLength);
  mac_address.ToBytes(absl::MakeSpan(
      reinterpret_cast<uint8_t*>(address_bytes.data()), address_bytes.size()));
  return address_bytes;
}

}  // namespace nearby
