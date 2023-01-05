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

#ifndef CORE_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
#define CORE_INTERNAL_WIFI_LAN_SERVICE_INFO_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/pcp.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/nsd_service_info.h"

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

  // The key of TXTRecord for EndpointInfo.
  static constexpr absl::string_view kKeyEndpointInfo{"n"};
  static constexpr std::uint32_t kServiceIdHashLength = 3;
  static constexpr int kMaxEndpointInfoLength = 131;

  WifiLanServiceInfo() = default;
  WifiLanServiceInfo(Version version, Pcp pcp, absl::string_view endpoint_id,
                     const ByteArray& service_id_hash,
                     const ByteArray& endpoint_info,
                     const ByteArray& uwb_address, WebRtcState web_rtc_state);

  // Constructs WifiLanServiceInfo through NsdServiceInfo.
  explicit WifiLanServiceInfo(const NsdServiceInfo& nsd_service_info);
  WifiLanServiceInfo(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo& operator=(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo(WifiLanServiceInfo&&) = default;
  WifiLanServiceInfo& operator=(WifiLanServiceInfo&&) = default;
  ~WifiLanServiceInfo() = default;

  explicit operator NsdServiceInfo() const;

  bool IsValid() const { return !endpoint_id_.empty(); }
  Version GetVersion() const { return version_; }
  Pcp GetPcp() const { return pcp_; }
  std::string GetEndpointId() const { return endpoint_id_; }
  ByteArray GetEndpointInfo() const { return endpoint_info_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray GetUwbAddress() const { return uwb_address_; }
  WebRtcState GetWebRtcState() const { return web_rtc_state_; }

 private:
  static constexpr int kMinLanServiceNameLength = 9;
  static constexpr int kEndpointIdLength = 4;
  static constexpr int kUwbAddressLengthSize = 1;
  static constexpr int kExtraFieldLength = 1;

  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kVersionShift = 5;
  static constexpr int kWebRtcConnectableFlagBitmask = 0x01;

  Version version_{Version::kUndefined};
  Pcp pcp_{Pcp::kUnknown};
  std::string endpoint_id_;
  ByteArray service_id_hash_;
  ByteArray endpoint_info_;
  // TODO(b/169550050): Define UWB address field.
  ByteArray uwb_address_;
  WebRtcState web_rtc_state_{WebRtcState::kUndefined};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
