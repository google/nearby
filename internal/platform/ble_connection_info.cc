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

#include "internal/platform/ble_connection_info.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace {
constexpr int kBleMacAddressMask = 0b01000000;
constexpr int kGattCharacteristicMask = 0b00100000;
constexpr int kPsmMask = 0b00010000;
constexpr int kPsmSize = 2;
}  // namespace

std::string BleConnectionInfo::ToDataElementBytes() const {
  std::string payload_data;
  payload_data.push_back(kBleGattMediumType);
  bool has_mac = mac_address_.size() == kMacAddressLength;
  bool has_gatt = !gatt_characteristic_.empty();
  bool has_psm = psm_.size() == kPsmSize;
  char mask = has_mac ? kBleMacAddressMask : 0;
  mask |= has_gatt ? kGattCharacteristicMask : 0;
  mask |= has_psm ? kPsmMask : 0;
  payload_data.push_back(mask);
  if (has_mac) {
    payload_data.append(mac_address_.begin(),
                        mac_address_.end());
  }
  if (has_gatt) {
    payload_data.push_back(gatt_characteristic_.length());
    payload_data.append(gatt_characteristic_.begin(),
                        gatt_characteristic_.end());
  }
  if (has_psm) {
    payload_data.append(psm_.begin(), psm_.end());
  }
  payload_data.append(actions_.begin(), actions_.end());
  std::string ret;
  ret.push_back(kDataElementFieldType);
  ret.push_back(payload_data.size());
  ret.append(payload_data.begin(), payload_data.end());
  return ret;
}

absl::StatusOr<BleConnectionInfo> BleConnectionInfo::FromDataElementBytes(
    absl::string_view bytes) {
  if (bytes.size() < kConnectionInfoMinimumLength) {
    return absl::InvalidArgumentError("Insufficient length of data element");
  }
  if (bytes[0] != kDataElementFieldType) {
    return absl::InvalidArgumentError("Not a data element type");
  }
  int position = 1;
  int length = static_cast<int>(bytes[position]);
  if (length != bytes.size() - 2) {
    return absl::InvalidArgumentError("Bad data element length");
  }
  if (bytes[++position] != kBleGattMediumType) {
    return absl::InvalidArgumentError("Not a BLE data element");
  }
  char mask = bytes[++position];
  std::string address;
  ++position;
  if ((mask & kBleMacAddressMask) == kBleMacAddressMask) {
    if (bytes.size() - position < kMacAddressLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read MAC address.");
    }
    address = std::string(bytes.substr(position, kMacAddressLength));
    position += kMacAddressLength;
  }
  std::string characteristic;
  if ((mask & kGattCharacteristicMask) == kGattCharacteristicMask) {
    char characteristic_length = bytes[position];
    if (bytes.size() - (position + 1) < characteristic_length) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read GATT characteristic.");
    }
    characteristic =
        std::string(bytes.substr(position + 1, characteristic_length));
    position += characteristic_length + 1;
  }
  std::string psm;
  if ((mask & kPsmMask) == kPsmMask) {
    if (bytes.size() - position < kPsmSize) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read PSM.");
    }
    psm = std::string(bytes.substr(position, kPsmSize));
    position += kPsmSize;
  }
  if (bytes.size() == position) {
    return absl::InvalidArgumentError(
        "Insufficient remaining bytes to read action.");
  }
  auto action_str = bytes.substr(position);
  return BleConnectionInfo(
      address, characteristic, psm,
      std::vector<uint8_t>(action_str.begin(), action_str.end()));
}
}  // namespace nearby
