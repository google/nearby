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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_LAN_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_LAN_CONNECTION_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {

inline constexpr int kIpv4AddressLength = 4;
inline constexpr int kIpv6AddressLength = 16;
inline constexpr int kPortLength = 2;
inline constexpr int kBssidLength = 6;

class WifiLanConnectionInfo : public ConnectionInfo {
 public:
  static absl::StatusOr<WifiLanConnectionInfo> FromDataElementBytes(
      absl::string_view bytes);

  WifiLanConnectionInfo(absl::string_view ip_address, absl::string_view port,
                        std::vector<uint8_t> actions)
      : ip_address_(ip_address), port_(port), bssid_(""), actions_(actions) {}
  WifiLanConnectionInfo(absl::string_view ip_address, absl::string_view port,
                        absl::string_view bssid, std::vector<uint8_t> actions)
      : ip_address_(ip_address),
        port_(port),
        bssid_(std::string(bssid)),
        actions_(actions) {}

  ::location::nearby::proto::connections::Medium GetMediumType()
      const override {
    return ::location::nearby::proto::connections::Medium::WIFI_LAN;
  }
  std::string ToDataElementBytes() const override;
  std::string GetIpAddress() const { return ip_address_; }
  // This port is expected to be in hex form, such as \xFF\xFF for a value of
  // 65535, 2 bytes in length. This field will be represented in network byte
  // order (aka big-endian), so \x12\x34 will correspond to port 4660 (0x1234).
  std::string GetPort() const { return port_; }
  std::string GetBssid() const { return bssid_; }
  std::vector<uint8_t> GetActions() const override { return actions_; }

 private:
  std::string ip_address_;
  std::string port_;
  std::string bssid_;
  std::vector<uint8_t> actions_;
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
