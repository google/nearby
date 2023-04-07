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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
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
    payload_data.insert(payload_data.end(), mac_address_.begin(),
                        mac_address_.end());
  }
  if (has_bluetooth_uuid) {
    payload_data.insert(payload_data.end(), bluetooth_uuid_.begin(),
                        bluetooth_uuid_.end());
  }
  payload_data.push_back(actions_);
  std::string ret;
  ret.push_back(kDataElementFieldType);
  ret.push_back(payload_data.size());
  ret.insert(ret.end(), payload_data.begin(), payload_data.end());
  return std::string(ret.data(), ret.size());
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
        "Insufficient remaining bytes to read action.");
  }
  char action = bytes[position];
  // Check that we don't have any remaining bytes.
  if (bytes.size() != ++position) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Nonzero remaining bytes: %d.", bytes.size() - position));
  }
  return BluetoothConnectionInfo(address, uuid, action);
}
}  // namespace nearby
