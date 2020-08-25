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

#ifndef CORE_V2_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
#define CORE_V2_INTERNAL_WIFI_LAN_SERVICE_INFO_H_

#include <cstdint>

#include "core_v2/internal/pcp.h"
#include "platform_v2/base/byte_array.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {

// Represents the format of the WifiLan service info used in Advertising +
// Discovery.
//
// See go/nearby-offline-data-interchange-formats for the specification.
class WifiLanServiceInfo {
 public:
  // Versions of the WifiLanServiceInfo.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
  };

  static constexpr std::uint32_t kServiceIdHashLength = 3;

  WifiLanServiceInfo() = default;
  WifiLanServiceInfo(Version version, Pcp pcp, absl::string_view endpoint_id,
                     const ByteArray& service_id_hash,
                     const ByteArray& endpoint_info);
  explicit WifiLanServiceInfo(absl::string_view service_info_string);
  WifiLanServiceInfo(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo& operator=(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo(WifiLanServiceInfo&&) = default;
  WifiLanServiceInfo& operator=(WifiLanServiceInfo&&) = default;
  ~WifiLanServiceInfo() = default;

  explicit operator std::string() const;

  bool IsValid() const { return !endpoint_id_.empty(); }
  Version GetVersion() const { return version_; }
  Pcp GetPcp() const { return pcp_; }
  std::string GetEndpointId() const { return endpoint_id_; }
  ByteArray GetEndpointInfo() const { return endpoint_info_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }

 private:
  // The maximum length of encrypted WifiLanServiceInfo string.
  static constexpr int kMaxLanServiceNameLength = 47;
  // The minimum length of encrypted WifiLanServiceInfo string.
  static constexpr int kMinLanServiceNameLength = 9;
  // The length for endpoint id in encrypted WifiLanServiceInfo string.
  static constexpr int kEndpointIdLength = 4;
  // The maximum length for endpoint id in encrypted WifiLanServiceInfo string.
  static constexpr int kMaxEndpointInfoLength = 131;

  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kVersionShift = 5;

  // WifiLanServiceInfo version.
  Version version_ = Version::kUndefined;
  // Pre-Connection Protocols version.
  Pcp pcp_ = Pcp::kUnknown;
  // Connected endpoint id.
  std::string endpoint_id_;
  // Connected hash service id.
  ByteArray service_id_hash_;
  // Connected endpoint info.
  ByteArray endpoint_info_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
