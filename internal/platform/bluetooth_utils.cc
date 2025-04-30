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
#include "internal/platform/byte_array.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace {

uint64_t ByteArrayToUint64(const ByteArray& byte_array) {
  if (byte_array.size() != BluetoothUtils::kBluetoothMacAddressLength) {
    return 0;
  }
  uint64_t result = 0;
  for (int i = 0; i < byte_array.size(); i++) {
    result <<= 8;
    result |= ((static_cast<uint64_t>(byte_array.data()[i])) & 0xFF);
  }
  return result;
}

ByteArray Uint64AddressToByteArray(uint64_t address) {
  ByteArray address_bytes(BluetoothUtils::kBluetoothMacAddressLength);
  address_bytes.data()[0] = (address >> 40) & 0xFF;
  address_bytes.data()[1] = (address >> 32) & 0xFF;
  address_bytes.data()[2] = (address >> 24) & 0xFF;
  address_bytes.data()[3] = (address >> 16) & 0xFF;
  address_bytes.data()[4] = (address >> 8) & 0xFF;
  address_bytes.data()[5] = address & 0xFF;
  return address_bytes;
}

}  // namespace

std::string BluetoothUtils::ToString(const ByteArray& bluetooth_mac_address) {
  MacAddress mac_address;
  if (!MacAddress::FromUint64(ByteArrayToUint64(bluetooth_mac_address),
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
  return Uint64AddressToByteArray(mac_address.address());
}

bool BluetoothUtils::IsBluetoothMacAddressUnset(
    const ByteArray& bluetooth_mac_address_bytes) {
  return ByteArrayToUint64(bluetooth_mac_address_bytes) == 0;
}

std::string BluetoothUtils::FromNumber(std::uint64_t address) {
  MacAddress mac_address;
  if (!MacAddress::FromUint64(address, mac_address) || !mac_address.IsSet()) {
    return "";
  }
  return mac_address.ToString();
}

std::uint64_t BluetoothUtils::ToNumber(absl::string_view address) {
  MacAddress mac_address;
  if (!MacAddress::FromString(address, mac_address)) {
    return 0;
  }
  return mac_address.address();
}

}  // namespace nearby
