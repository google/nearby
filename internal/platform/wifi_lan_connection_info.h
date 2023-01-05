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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_LAN_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_LAN_CONNECTION_INFO_H_

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/connection_info.h"

namespace nearby {

constexpr int kIpv4AddressLength = 4;
constexpr int kIpv6AddressLength = 16;
constexpr int kPortLength = 2;
constexpr int kBssidLength = 6;

class WifiLanConnectionInfo : public ConnectionInfo {
 public:
  enum WifiLanConnectionInfoType {
    kUnknown = 0,
    kIpv4 = 1,
    kIpv6 = 2,
  };
  WifiLanConnectionInfo(absl::string_view ip_address, absl::string_view port)
      : ip_address_(ip_address), port_(port), bssid_("") {
    port_.resize(kPortLength);
    bssid_.resize(kBssidLength);
  }
  WifiLanConnectionInfo(absl::string_view ip_address, absl::string_view port,
                        absl::string_view bssid)
      : ip_address_(ip_address), port_(port), bssid_(std::string(bssid)) {
    port_.resize(kPortLength);
    bssid_.resize(kBssidLength);
  }
  static absl::StatusOr<WifiLanConnectionInfo> FromBytes(ByteArray bytes);
  MediumType GetMediumType() const override { return MediumType::kWifiLan; }
  ByteArray ToBytes() const override;
  ByteArray GetIpAddress() const { return ByteArray(ip_address_); }
  ByteArray GetPort() const { return ByteArray(port_); }
  ByteArray GetBssid() const { return ByteArray(bssid_); }
  WifiLanConnectionInfoType GetWifiLanConnectionInfoType() const {
    if (ip_address_.size() == kIpv6AddressLength) {
      return kIpv6;
    } else if (ip_address_.size() == kIpv4AddressLength) {
      return kIpv4;
    }
    return kUnknown;
  }

 private:
  std::string ip_address_;
  std::string port_;
  std::string bssid_;
};

inline bool operator==(const WifiLanConnectionInfo& a,
                       const WifiLanConnectionInfo& b) {
  return a.GetIpAddress() == b.GetIpAddress() && a.GetPort() == b.GetPort() &&
         a.GetBssid() == b.GetBssid();
}

inline bool operator!=(const WifiLanConnectionInfo& a,
                       const WifiLanConnectionInfo& b) {
  return !(a == b);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_LAN_CONNECTION_INFO_H_
