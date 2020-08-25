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
