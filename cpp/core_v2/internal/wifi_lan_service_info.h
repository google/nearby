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
                     absl::string_view endpoint_name);
  explicit WifiLanServiceInfo(absl::string_view service_info_string);
  ~WifiLanServiceInfo() = default;

  WifiLanServiceInfo(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo& operator=(const WifiLanServiceInfo&) = default;
  WifiLanServiceInfo(WifiLanServiceInfo&&) = default;
  WifiLanServiceInfo& operator=(WifiLanServiceInfo&&) = default;

  explicit operator std::string() const;

  inline bool IsValid() const { return !endpoint_id_.empty(); }
  inline Version GetVersion() const { return version_; }
  inline Pcp GetPcp() const { return pcp_; }
  inline std::string GetEndpointId() const { return endpoint_id_; }
  inline std::string GetEndpointName() const { return endpoint_name_; }
  inline ByteArray GetServiceIdHash() const { return service_id_hash_; }

 private:
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

  // WifiLanServiceInfo version.
  Version version_ = Version::kUndefined;
  // Pre-Connection Protocols version.
  Pcp pcp_ = Pcp::kUnknown;
  // Connected endpoint id.
  std::string endpoint_id_;
  // Connected hash service id.
  ByteArray service_id_hash_;
  // TODO(edwinwu): Replaces endpointName as endPointInfo eventually;
  // it is not in this version yet for endpointName.
  // Connected endpoint name.
  std::string endpoint_name_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_WIFI_LAN_SERVICE_INFO_H_
