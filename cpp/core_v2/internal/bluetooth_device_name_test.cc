#include "core_v2/internal/bluetooth_device_name.h"

#include <cstring>
#include <memory>

#include "platform_v2/base/base64_utils.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const BluetoothDeviceName::Version kVersion = BluetoothDeviceName::Version::kV1;
const Pcp kPcp = Pcp::kP2pCluster;
// TODO(edwinwu): Replace absl::string_view in other medium tests, too.
inline constexpr absl::string_view kEndPointID = "AB12";
inline constexpr absl::string_view kServiceIDHashBytes = "\x0a\x0b\x0c";
inline constexpr absl::string_view kEndPointName = "RAWK + ROWL!";

TEST(BluetoothDeviceNameTest, ConstructionWorks) {
  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{kVersion, kPcp, kEndPointID,
                                            service_id_hash, kEndPointName};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(kVersion, bluetooth_device_name.GetVersion());
  EXPECT_EQ(kPcp, bluetooth_device_name.GetPcp());
  EXPECT_EQ(kEndPointID, bluetooth_device_name.GetEndpointId());
  EXPECT_EQ(service_id_hash, bluetooth_device_name.GetServiceIdHash());
  EXPECT_EQ(kEndPointName, bluetooth_device_name.GetEndpointName());
}

TEST(BluetoothDeviceNameTest, ConstructionWorksWithEmptyEndpointName) {
  std::string empty_endpoint_name;

  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{
      kVersion, kPcp, kEndPointID, service_id_hash, empty_endpoint_name};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(kVersion, bluetooth_device_name.GetVersion());
  EXPECT_EQ(kPcp, bluetooth_device_name.GetPcp());
  EXPECT_EQ(kEndPointID, bluetooth_device_name.GetEndpointId());
  EXPECT_EQ(service_id_hash, bluetooth_device_name.GetServiceIdHash());
  EXPECT_EQ(empty_endpoint_name, bluetooth_device_name.GetEndpointName());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BluetoothDeviceName::Version>(666);

  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{bad_version, kPcp, kEndPointID,
                                            service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadPcp) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{kVersion, bad_pcp, kEndPointID,
                                            service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{kVersion, kPcp, short_endpoint_id,
                                            service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{kVersion, kPcp, long_endpoint_id,
                                            service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = "\x0a\x0b";

  ByteArray short_service_id_hash{short_service_id_hash_bytes};
  BluetoothDeviceName bluetooth_device_name{
      kVersion, kPcp, kEndPointID, short_service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = "\x0a\x0b\x0c\x0d";

  ByteArray long_service_id_hash{long_service_id_hash_bytes};
  BluetoothDeviceName bluetooth_device_name{
      kVersion, kPcp, kEndPointID, long_service_id_hash, kEndPointName};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithShortStringLength) {
  char bluetooth_device_name_string[] = "X";

  ByteArray bluetooth_device_name_bytes{bluetooth_device_name_string};
  BluetoothDeviceName bluetooth_device_name{
      Base64Utils::Encode(bluetooth_device_name_bytes)};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithWrongEndpointNameLength) {
  // Serialize good data into a good Bluetooth Device Name.
  ByteArray service_id_hash{kServiceIDHashBytes};
  BluetoothDeviceName bluetooth_device_name{kVersion, kPcp, kEndPointID,
                                            service_id_hash, kEndPointName};
  auto bluetooth_device_name_string = std::string(bluetooth_device_name);

  // Base64-decode the good Bluetooth Device Name.
  ByteArray bluetooth_device_name_bytes =
      Base64Utils::Decode(bluetooth_device_name_string);
  // Corrupt the EndpointNameLength bits (120-127) by reversing all of them.
  std::string corrupt_string(bluetooth_device_name_bytes.data(),
                             bluetooth_device_name_bytes.size());
  corrupt_string[15] ^= 0x0FF;
  // Base64-encode the corrupted bytes into a corrupt Bluetooth Device Name.
  ByteArray corrupt_bluetooth_device_name_bytes{corrupt_string.data(),
                                                corrupt_string.size()};
  std::string corrupt_bluetooth_device_name_string(
      Base64Utils::Encode(corrupt_bluetooth_device_name_bytes));

  // And deserialize the corrupt Bluetooth Device Name.
  BluetoothDeviceName corrupt_bluetooth_device_name(
      corrupt_bluetooth_device_name_string);

  EXPECT_TRUE(corrupt_bluetooth_device_name.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
