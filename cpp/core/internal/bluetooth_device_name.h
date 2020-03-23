#ifndef CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_
#define CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_

#include <cstdint>

#include "core/internal/pcp.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

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
  struct Version {
    enum Value {
      V1 = 1,
      // Version is only allocated 3 bits in the BluetoothDeviceName, so this
      // can never go beyond V7.
    };
  };

  static Ptr<BluetoothDeviceName> fromString(
      const std::string& bluetooth_device_name_string);

  static std::string asString(Version::Value version, PCP::Value pcp,
                              const std::string& endpoint_id,
                              ConstPtr<ByteArray> service_id_hash,
                              const std::string& endpoint_name);

  static const std::uint32_t kServiceIdHashLength;

  ~BluetoothDeviceName();

  Version::Value getVersion() const;
  PCP::Value getPCP() const;
  std::string getEndpointId() const;
  ConstPtr<ByteArray> getServiceIdHash() const;
  std::string getEndpointName() const;

 private:
  static Ptr<BluetoothDeviceName> createV1BluetoothDeviceName(
      ConstPtr<ByteArray> bluetooth_device_name_bytes);
  static std::uint32_t computeEndpointNameLength(
      ConstPtr<ByteArray> bluetooth_device_name_bytes);
  static std::uint32_t computeBluetoothDeviceNameLength(
      ConstPtr<ByteArray> endpoint_name_bytes);
  static Ptr<ByteArray> createV1Bytes(PCP::Value pcp,
                                      const std::string& endpoint_id,
                                      ConstPtr<ByteArray> service_id_hash,
                                      ConstPtr<ByteArray> endpoint_name_bytes);

  static const std::uint32_t kMaxBluetoothDeviceNameLength;
  static const std::uint32_t kEndpointIdLength;
  static const std::uint32_t kReservedLength;
  static const std::uint32_t kMaxEndpointNameLength;
  static const std::uint32_t kMinBluetoothDeviceNameLength;

  static const std::uint16_t kVersionBitmask;
  static const std::uint16_t kPCPBitmask;
  static const std::uint16_t kEndpointNameLengthBitmask;

  BluetoothDeviceName(Version::Value version, PCP::Value pcp,
                      const std::string& endpoint_id,
                      ConstPtr<ByteArray> service_id_hash,
                      const std::string& endpoint_name);

  const Version::Value version_;
  const PCP::Value pcp_;
  const std::string endpoint_id_;
  ScopedPtr<ConstPtr<ByteArray> > service_id_hash_;
  const std::string endpoint_name_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BLUETOOTH_DEVICE_NAME_H_
