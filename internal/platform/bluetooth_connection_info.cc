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

#include "internal/platform/bluetooth_connection_info.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace {
constexpr int kMacAddressMask = 0b01000000;
constexpr int kBluetoothUuidMask = 0b00100000;
}  // namespace

std::string BluetoothConnectionInfo::ToDataElementBytes() const {
  std::string payload_data;
  payload_data.push_back(kBluetoothMediumType);
  bool has_mac = mac_address_.size() == kMacAddressLength;
  bool has_bluetooth_uuid = !bluetooth_uuid_.empty();
  char mask = has_mac ? kMacAddressMask : 0;
  mask |= has_bluetooth_uuid ? kBluetoothUuidMask : 0;
  payload_data.push_back(mask);
  if (has_mac) {
    payload_data.append(mac_address_.begin(), mac_address_.end());
  }
  if (has_bluetooth_uuid) {
    payload_data.append(bluetooth_uuid_.begin(), bluetooth_uuid_.end());
  }
  payload_data.append(actions_.begin(), actions_.end());
  std::string ret;
  ret.push_back(kDataElementFieldType);
  ret.push_back(payload_data.size());
  ret.append(payload_data.begin(), payload_data.end());
  return ret;
}

absl::StatusOr<BluetoothConnectionInfo>
BluetoothConnectionInfo::FromDataElementBytes(absl::string_view bytes) {
  if (bytes.size() < kConnectionInfoMinimumLength) {
    return absl::InvalidArgumentError("Insufficient length of data element");
  }
  if (bytes[0] != kDataElementFieldType) {
    return absl::InvalidArgumentError("Not a data element type");
  }
  int position = 1;
  char length = bytes[position];
  if (length != bytes.size() - 2) {
    return absl::InvalidArgumentError("Bad data element length");
  }
  if (bytes[++position] != kBluetoothMediumType) {
    return absl::InvalidArgumentError("Not a Bluetooth data element");
  }
  char mask = bytes[++position];
  std::string address = "";
  ++position;
  if ((mask & kMacAddressMask) == kMacAddressMask) {
    if (bytes.size() - position < kMacAddressLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read MAC address.");
    }
    address = std::string(bytes.substr(position, kMacAddressLength));
    position += kMacAddressLength;
  }
  std::string uuid = "";
  if ((mask & kBluetoothUuidMask) == kBluetoothUuidMask) {
    if (bytes.size() - position < 4) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read bluetooth UUID.");
    }
    uuid = std::string(bytes.substr(position, 4));
    position += 4;
  }
  if (bytes.size() == position) {
    return absl::InvalidArgumentError(
        "Insufficient remaining bytes to read actions.");
  }
  auto action_str = bytes.substr(position);
  return BluetoothConnectionInfo(
      address, uuid,
      std::vector<uint8_t>(action_str.begin(), action_str.end()));
}
}  // namespace nearby
