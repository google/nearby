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

#include "core/internal/pcp.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
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
    kV1 = 1,
  };

  // Static method to deserialize from the encrypted string to
  // WifiLanServiceInfo object.
  // TODO(b/149762166): Ptr is deprectaed. Uses shrared_ptr<T> or unique_ptr<T>.
  static Ptr<WifiLanServiceInfo> FromString(
      absl::string_view wifi_lan_service_info_string);

  // Static method to serialize to encrypted string from WifiLanServiceInfo
  // object.
  static std::string AsString(Version version, PCP::Value pcp,
                              absl::string_view endpoint_id,
                              ConstPtr<ByteArray> service_id_hash);

  static constexpr std::uint32_t kServiceIdHashLength = 3;

  ~WifiLanServiceInfo() = default;

  inline Version GetVersion() const { return version_; }
  inline PCP::Value GetPcp() const { return pcp_; }
  inline std::string GetEndpointId() const { return endpoint_id_; }
  inline ConstPtr<ByteArray> GetServiceIdHash() const {
    return service_id_hash_.get();
  }
  inline std::string GetEndpointName() const { return endpoint_name_; }

 private:
  static Ptr<WifiLanServiceInfo> CreateV1WifiLanServiceInfo(
      ConstPtr<ByteArray> wifi_lan_service_info_name_bytes);
  static std::uint32_t ComputeEndpointNameLength(
      ConstPtr<ByteArray> wifi_lan_service_info_name_bytes);
  static Ptr<ByteArray> CreateV1Bytes(PCP::Value pcp,
                                      absl::string_view endpoint_id,
                                      ConstPtr<ByteArray> service_id_hash);

  // The maximum length of encrypted WifiLanServiceInfo string.
  static constexpr int kMaxLanServiceNameLength = 47;
  // The minimum length of encrypted WifiLanServiceInfo string.
  static constexpr int kMinLanServiceNameLength = 9;
  // The length for endpoint id in encrypted WifiLanServiceInfo string.
  static constexpr int kEndpointIdLength = 4;
  // The maximum length for endpoint id in encrypted WifiLanServiceInfo string.
  static constexpr int kMaxEndpointNameLength = 131;

  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kVersionShift = 5;

  WifiLanServiceInfo(Version version, PCP::Value pcp,
                     absl::string_view endpoint_id,
                     ConstPtr<ByteArray> service_id_hash,
                     absl::string_view endpoint_name);

  // WifiLanServiceInfo version.
  const Version version_;
  // Pre-Connection Protocols version.
  const PCP::Value pcp_;
  // Connected endpoint id.
  const std::string endpoint_id_;
  // Connected hash service id.
  ScopedPtr<ConstPtr<ByteArray> > service_id_hash_;
  // TODO(b/149806065): Replaces endpointName as endPointInfo eventually;
  // it is not in this version yet for endpointName.
  // Connected endpoint name.
  const std::string endpoint_name_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
