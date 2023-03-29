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

#include "internal/platform/wifi_lan_connection_info.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace {
constexpr char kIpv4Mask = 0b01000000;
constexpr char kIpv6Mask = 0b00100000;
constexpr char kPortMask = 0b00010000;
constexpr char kBssidMask = 0b00001000;
}  // namespace

std::string WifiLanConnectionInfo::ToDataElementBytes() const {
  std::string payload_data;
  payload_data.push_back(kWifiLanMediumType);
  bool has_ipv4 = ip_address_.size() == kIpv4AddressLength;
  bool has_ipv6 = ip_address_.size() == kIpv6AddressLength;
  bool has_bssid = bssid_.size() == kBssidLength;
  char mask = kPortMask;
  mask |= has_ipv4 ? kIpv4Mask : 0;
  mask |= has_ipv6 ? kIpv6Mask : 0;
  mask |= has_bssid ? kBssidMask : 0;
  payload_data.push_back(mask);
  if (has_ipv4 || has_ipv6) {
    payload_data.insert(payload_data.end(), ip_address_.begin(),
                        ip_address_.end());
  }
  payload_data.insert(payload_data.end(), port_.begin(), port_.end());
  if (has_bssid) {
    payload_data.insert(payload_data.end(), bssid_.begin(), bssid_.end());
  }
  payload_data.push_back(actions_);
  std::string ret;
  ret.push_back(kDataElementFieldType);
  ret.push_back(payload_data.size());
  ret.insert(ret.end(), payload_data.begin(), payload_data.end());
  return std::string(ret.data(), ret.size());
}

absl::StatusOr<WifiLanConnectionInfo>
WifiLanConnectionInfo::FromDataElementBytes(absl::string_view bytes) {
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
  if (bytes[++position] != kWifiLanMediumType) {
    return absl::InvalidArgumentError("Not a WiFi LAN data element");
  }
  char mask = bytes[++position];
  std::string address;
  ++position;
  // Sanity check to make sure that both are not present.
  if ((mask & kIpv4Mask) == kIpv4Mask && (mask & kIpv6Mask) == kIpv6Mask) {
    return absl::InvalidArgumentError(
        "Both IPV4 and IPV6 addresses cannot be present.");
  }
  if ((mask & kIpv4Mask) == kIpv4Mask) {
    if (bytes.size() - position < kIpv4AddressLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read IPV4 address.");
    }
    address = std::string(bytes.substr(position, kIpv4AddressLength));
    position += kIpv4AddressLength;
  } else if ((mask & kIpv6Mask) == kIpv6Mask) {
    if (bytes.size() - position < kIpv6AddressLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read IPV6 address.");
    }
    address = std::string(bytes.substr(position, kIpv6AddressLength));
    position += kIpv6AddressLength;
  }

  std::string port;
  if ((mask & kPortMask) == kPortMask) {
    if (bytes.size() - position < kPortLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read port.");
    }
    port = std::string(bytes.substr(position, kPortLength));
    position += kPortLength;
  }

  std::string bssid;
  if ((mask & kBssidMask) == kBssidMask) {
    if (bytes.size() - position < kBssidLength) {
      return absl::InvalidArgumentError(
          "Insufficient remaining bytes to read port.");
    }
    bssid = std::string(bytes.substr(position, kBssidLength));
    position += kBssidLength;
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
  return WifiLanConnectionInfo(address, port, bssid, action);
}

}  // namespace nearby
