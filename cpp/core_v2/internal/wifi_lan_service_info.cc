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

#include "core_v2/internal/wifi_lan_service_info.h"

#include <inttypes.h>

#include <cstring>
#include <utility>

#include "platform_v2/base/base64_utils.h"
#include "platform_v2/public/logging.h"

namespace location {
namespace nearby {
namespace connections {

WifiLanServiceInfo::WifiLanServiceInfo(Version version, Pcp pcp,
                                       absl::string_view endpoint_id,
                                       const ByteArray& service_id_hash,
                                       absl::string_view endpoint_name) {
  if (version != Version::kV1 || endpoint_id.empty() ||
      endpoint_id.length() != kEndpointIdLength ||
      service_id_hash.size() != kServiceIdHashLength) {
    return;
  }
  switch (pcp) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      break;
    default:
      return;
  }

  version_ = version;
  pcp_ = pcp;
  service_id_hash_ = service_id_hash;
  endpoint_id_ = endpoint_id;
}

WifiLanServiceInfo::WifiLanServiceInfo(absl::string_view service_info_string) {
  ByteArray service_info_bytes = Base64Utils::Decode(service_info_string);

  if (service_info_bytes.Empty()) {
    NEARBY_LOG(
        ERROR,
        "Cannot deserialize WifiLanServiceInfo: failed Base64 decoding of %s",
        std::string(service_info_string).c_str());
    return;
  }

  if (service_info_bytes.size() > kMaxLanServiceNameLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize WifiLanServiceInfo: expecting max %d raw "
               "bytes, got %" PRIu64,
               kMaxLanServiceNameLength, service_info_bytes.size());
    return;
  }

  if (service_info_bytes.size() < kMinLanServiceNameLength) {
    NEARBY_LOG(ERROR,
               "Cannot deserialize WifiLanServiceInfo: expecting min %d raw "
               "bytes, got %" PRIu64,
               kMinLanServiceNameLength, service_info_bytes.size());
    return;
  }

  // The upper 3 bits are supposed to be the version.
  version_ = static_cast<Version>(
      (service_info_bytes.data()[0] & kVersionBitmask) >> kVersionShift);
  const char* service_info_bytes_read_ptr = service_info_bytes.data();
  switch (version_) {
    case Version::kV1:
      // The lower 5 bits of the V1 payload are supposed to be the Pcp.
      pcp_ = static_cast<Pcp>(*service_info_bytes_read_ptr & kPcpBitmask);
      service_info_bytes_read_ptr++;
      switch (pcp_) {
        case Pcp::kP2pCluster:  // Fall through
        case Pcp::kP2pStar:     // Fall through
        case Pcp::kP2pPointToPoint:
          // The next 32 bits are supposed to be the endpoint_id.
          endpoint_id_ =
              std::string(service_info_bytes_read_ptr, kEndpointIdLength);
          service_info_bytes_read_ptr += kEndpointIdLength;

          // The next 24 bits are supposed to be the service_id_hash.
          service_id_hash_ =
              ByteArray(service_info_bytes_read_ptr, kServiceIdHashLength);
          service_info_bytes_read_ptr += kServiceIdHashLength;

          // The next bits are supposed to be endpoint_name.
          // TODO(edwinwu): Implements it. Temp to set "found_device".
          endpoint_name_ = "found_device";
          break;

        default:
          // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
          // the air, or older versions of GmsCore intermingling with newer
          // ones.
          NEARBY_LOG(
              ERROR,
              "Cannot deserialize WifiLanServiceInfo: unsupported V1 PCP %d",
              pcp_);
          break;
      }
      break;

    default:
      // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer ones.
      NEARBY_LOG(
          ERROR,
          "Cannot deserialize WifiLanServiceInfo: unsupported Version %d",
          version_);
      break;
  }
}

WifiLanServiceInfo::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  ByteArray wifi_lan_service_info_name_bytes(kMinLanServiceNameLength);
  auto* wifi_lan_service_info_name_bytes_write_ptr =
      wifi_lan_service_info_name_bytes.data();

  // The upper 3 bits are the Version.
  auto version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |=
      static_cast<char>(static_cast<uint32_t>(pcp_) & kPcpBitmask);
  *wifi_lan_service_info_name_bytes_write_ptr = version_and_pcp_byte;
  wifi_lan_service_info_name_bytes_write_ptr++;

  switch (pcp_) {
    case Pcp::kP2pCluster:  // Fall through
    case Pcp::kP2pStar:     // Fall through
    case Pcp::kP2pPointToPoint:
      // The next 32 bits are the endpoint_id.
      if (endpoint_id_.size() != kEndpointIdLength) {
        NEARBY_LOG(
            ERROR,
            "Cannot serialize WifiLanServiceInfo: V1 Endpoint ID %s (%" PRIu64
            " bytes) should be exactly %d bytes",
            endpoint_id_.c_str(), endpoint_id_.size(), kEndpointIdLength);
        return "";
      }
      memcpy(wifi_lan_service_info_name_bytes_write_ptr, endpoint_id_.data(),
             kEndpointIdLength);
      wifi_lan_service_info_name_bytes_write_ptr += kEndpointIdLength;

      // The next 24 bits are the service_id_hash.
      if (service_id_hash_.size() != kServiceIdHashLength) {
        NEARBY_LOG(
            ERROR,
            "Cannot serialize WifiLanServiceInfo: V1 ServiceID hash (%" PRIu64
            " bytes) should be exactly %d bytes",
            service_id_hash_.size(), kServiceIdHashLength);
        return "";
      }
      memcpy(wifi_lan_service_info_name_bytes_write_ptr,
             service_id_hash_.data(), kServiceIdHashLength);
      wifi_lan_service_info_name_bytes_write_ptr += kServiceIdHashLength;

      // The next bits are the endpoint_name.
      // TODO(edwinwu): Implements to parse endpoint_name.
      break;
    default:
      NEARBY_LOG(ERROR,
                 "Cannot serialize WifiLanServiceInfo: unsupported V1 PCP %d",
                 pcp_);
      return "";
  }

  return Base64Utils::Encode(wifi_lan_service_info_name_bytes);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
