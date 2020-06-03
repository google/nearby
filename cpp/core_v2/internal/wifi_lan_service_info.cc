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
  endpoint_id_ = std::string(endpoint_id);
}

WifiLanServiceInfo::WifiLanServiceInfo(absl::string_view service_info_string) {
  ByteArray service_info_bytes = Base64Utils::Decode(service_info_string);

  if (service_info_bytes.Empty()) {
    NEARBY_LOG(
        INFO,
        "Cannot deserialize WifiLanServiceInfo: failed Base64 decoding of %s",
        std::string(service_info_string).c_str());
    return;
  }

  if (service_info_bytes.size() > kMaxLanServiceNameLength) {
    NEARBY_LOG(INFO,
               "Cannot deserialize WifiLanServiceInfo: expecting max %d raw "
               "bytes, got %" PRIu64,
               kMaxLanServiceNameLength, service_info_bytes.size());
    return;
  }

  if (service_info_bytes.size() < kMinLanServiceNameLength) {
    NEARBY_LOG(INFO,
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
              INFO,
              "Cannot deserialize WifiLanServiceInfo: unsupported V1 PCP %d",
              pcp_);
          break;
      }
      break;

    default:
      // TODO(edwinwu): [ANALYTICIZE] This either represents corruption over
      // the air, or older versions of GmsCore intermingling with newer ones.
      NEARBY_LOG(
          INFO, "Cannot deserialize WifiLanServiceInfo: unsupported Version %d",
          version_);
      break;
  }
}

WifiLanServiceInfo::operator std::string() const {
  if (!IsValid()) {
    return "";
  }

  std::string out;

  // The upper 3 bits are the Version.
  auto version_and_pcp_byte = static_cast<char>(
      (static_cast<uint32_t>(Version::kV1) << 5) & kVersionBitmask);
  // The lower 5 bits are the PCP.
  version_and_pcp_byte |=
      static_cast<char>(static_cast<uint32_t>(pcp_) & kPcpBitmask);

  out.reserve(kMinLanServiceNameLength);
  out.append(1, version_and_pcp_byte);
  out.append(endpoint_id_);
  out.append(std::string(service_id_hash_));
  // The last byte is reserved to fit the kMinLanServiceNameLength.
  out.append(" ");

  return Base64Utils::Encode(ByteArray{std::move(out)});
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
