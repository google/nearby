#ifndef CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_
#define CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_

#include <cstdint>

#include "core/internal/base_pcp_handler.h"
#include "core/internal/pcp.h"
#include "platform/base/byte_array.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {

// Represents the format of the Bluetooth device name used in Advertising +
// Discovery.
//
// <p>See go/nearby-offline-data-interchange-formats for the specification.
class BluetoothDeviceName {
 public:
  // Versions of the BluetoothDeviceName.
  enum class Version {
    kUndefined = 0,
    kV1 = 1,
    // Version is only allocated 3 bits in the BluetoothDeviceName, so this
    // can never go beyond V7.
  };

  static constexpr int kServiceIdHashLength = 3;

  BluetoothDeviceName() = default;
  BluetoothDeviceName(Version version, Pcp pcp, absl::string_view endpoint_id,
                      const ByteArray& service_id_hash,
                      const ByteArray& endpoint_info,
                      const ByteArray& uwb_address,
                      WebRtcState web_rtc_state);
  explicit BluetoothDeviceName(absl::string_view bluetooth_device_name_string);
  BluetoothDeviceName(const BluetoothDeviceName&) = default;
  BluetoothDeviceName& operator=(const BluetoothDeviceName&) = default;
  BluetoothDeviceName(BluetoothDeviceName&&) = default;
  BluetoothDeviceName& operator=(BluetoothDeviceName&&) = default;
  ~BluetoothDeviceName() = default;

  explicit operator std::string() const;

  bool IsValid() const { return !endpoint_id_.empty(); }
  Version GetVersion() const { return version_; }
  Pcp GetPcp() const { return pcp_; }
  std::string GetEndpointId() const { return endpoint_id_; }
  ByteArray GetServiceIdHash() const { return service_id_hash_; }
  ByteArray GetEndpointInfo() const { return endpoint_info_; }
  ByteArray GetUwbAddress() const { return uwb_address_; }
  WebRtcState GetWebRtcState() const { return web_rtc_state_; }

 private:
  static constexpr int kEndpointIdLength = 4;
  static constexpr int kReservedLength = 6;
  static constexpr int kMaxEndpointInfoLength = 131;
  static constexpr int kMinBluetoothDeviceNameLength = 16;

  static constexpr int kVersionBitmask = 0x0E0;
  static constexpr int kPcpBitmask = 0x01F;
  static constexpr int kEndpointNameLengthBitmask = 0x0FF;
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
}  // namespace location

#endif  // CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_
