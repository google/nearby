// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_AWARE_CONNECTION_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_AWARE_CONNECTION_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/connection_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {

inline constexpr char kServiceIdMask = 0b00010000;

class WifiAwareConnectionInfo : public ConnectionInfo {
 public:
  static absl::StatusOr<WifiAwareConnectionInfo> FromDataElementBytes(
      absl::string_view bytes);

  WifiAwareConnectionInfo(absl::string_view service_id,
                          std::vector<uint8_t> actions)
      : service_id_(service_id), actions_(actions) {}

  ::location::nearby::proto::connections::Medium GetMediumType()
      const override {
    return ::location::nearby::proto::connections::Medium::WIFI_AWARE;
  }
  std::string ToDataElementBytes() const override;
  std::string GetServiceId() const { return service_id_; }
  std::vector<uint8_t> GetActions() const override { return actions_; }

 private:
  std::string service_id_;
  std::vector<uint8_t> actions_;
};

inline bool operator==(const WifiAwareConnectionInfo& a,
                       const WifiAwareConnectionInfo& b) {
  return a.GetServiceId() == b.GetServiceId();
}

inline bool operator!=(const WifiAwareConnectionInfo& a,
                       const WifiAwareConnectionInfo& b) {
  return !(a == b);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_WIFI_AWARE_CONNECTION_INFO_H_
