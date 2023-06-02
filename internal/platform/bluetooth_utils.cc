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

#include <algorithm>

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

  bt_mac_address.erase(
      std::remove(bt_mac_address.begin(), bt_mac_address.end(), ':'),
      bt_mac_address.end());

  if (bt_mac_address.length() != kBluetoothMacAddressLength * 2 ||
      !std::all_of(bt_mac_address.begin(), bt_mac_address.end(),
                   absl::ascii_isxdigit)) {
    return ByteArray();
  }
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

std::string BluetoothUtils::FromNumber(std::uint64_t address) {
  // Gets byte `index` from `address`.
  auto b = [&](int index) {
    return (std::uint8_t)(address >> (56 - 8 * index));
  };
  return absl::StrFormat("%02X:%02X:%02X:%02X:%02X:%02X", b(2), b(3), b(4),
                         b(5), b(6), b(7));
}

std::uint64_t BluetoothUtils::ToNumber(absl::string_view address) {
  ByteArray binary = FromString(address);
  if (binary.size() != kBluetoothMacAddressLength) {
    return 0;
  }
  std::uint64_t result = 0;
  for (char ch : binary.AsStringView()) {
    result <<= 8;
    result |= static_cast<uint8_t>(ch);
  }

  return result;
}

}  // namespace nearby
