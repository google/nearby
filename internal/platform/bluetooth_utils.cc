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

#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"

namespace nearby {

std::string BluetoothUtils::ToString(const ByteArray& bluetooth_mac_address) {
  std::string colon_delimited_string;

  if (bluetooth_mac_address.size() != kBluetoothMacAddressLength)
    return colon_delimited_string;

  if (IsBluetoothMacAddressUnset(bluetooth_mac_address))
    return colon_delimited_string;

  for (auto byte : std::string(bluetooth_mac_address)) {
    if (!colon_delimited_string.empty())
      absl::StrAppend(&colon_delimited_string, ":");
    absl::StrAppend(&colon_delimited_string, absl::StrFormat("%02X", byte));
  }
  return colon_delimited_string;
}

ByteArray BluetoothUtils::FromString(absl::string_view bluetooth_mac_address) {
  std::string bt_mac_address(bluetooth_mac_address);

  // Remove the colon delimiters.
  bt_mac_address.erase(
      std::remove(bt_mac_address.begin(), bt_mac_address.end(), ':'),
      bt_mac_address.end());

  // If the bluetooth mac address is invalid (wrong size), return a null byte
  // array.
  if (bt_mac_address.length() != kBluetoothMacAddressLength * 2) {
    return ByteArray();
  }

  // Convert to bytes. If MAC Address bytes are unset, return a null byte array.
  auto bt_mac_address_string(absl::HexStringToBytes(bt_mac_address));
  auto bt_mac_address_bytes =
      ByteArray(bt_mac_address_string.data(), bt_mac_address_string.size());
  if (IsBluetoothMacAddressUnset(bt_mac_address_bytes)) {
    return ByteArray();
  }
  return bt_mac_address_bytes;
}

bool BluetoothUtils::IsBluetoothMacAddressUnset(
    const ByteArray& bluetooth_mac_address_bytes) {
  for (int i = 0; i < bluetooth_mac_address_bytes.size(); i++) {
    if (bluetooth_mac_address_bytes.data()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace nearby
