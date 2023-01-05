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

#include "internal/platform/wifi_lan_connection_info.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {

ByteArray WifiLanConnectionInfo::ToBytes() const {
  return ByteArray(absl::StrCat(std::to_string(GetWifiLanConnectionInfoType()),
                                ip_address_, port_, bssid_));
}

absl::StatusOr<WifiLanConnectionInfo> WifiLanConnectionInfo::FromBytes(
    ByteArray bytes) {
  // IPV4/IPV6 indicator size are both 1
  int ip_size = 1;
  absl::string_view serial = bytes.AsStringView();
  if (serial.length() !=
          ip_size + kIpv4AddressLength + kPortLength + kBssidLength &&
      serial.length() !=
          ip_size + kIpv6AddressLength + kPortLength + kBssidLength) {
    return absl::InvalidArgumentError("Bad byte array length");
  }
  size_t proc;
  int type = std::stoi(std::string(serial.substr(0, ip_size)), &proc);
  if (proc != ip_size) {
    return absl::InvalidArgumentError("Could not determine IPV4 or IPV6");
  }
  int ip_length = type == kIpv4 ? kIpv4AddressLength : kIpv6AddressLength;
  absl::string_view ip = serial.substr(ip_size, ip_length);
  absl::string_view port = serial.substr(ip_size + ip_length, kPortLength);
  absl::string_view bssid = serial.substr(ip_size + ip_length + kPortLength);
  return WifiLanConnectionInfo(ip, port, bssid);
}

}  // namespace nearby
