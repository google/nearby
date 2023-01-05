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

#include "connections/implementation/bluetooth_device_name.h"

#include <cstring>
#include <memory>

#include "gtest/gtest.h"
#include "internal/platform/base64_utils.h"

namespace nearby {
namespace connections {
namespace {

constexpr BluetoothDeviceName::Version kVersion =
    BluetoothDeviceName::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kEndPointID{"AB12"};
constexpr absl::string_view kServiceIDHashBytes{"\x0a\x0b\x0c"};
constexpr absl::string_view kEndPointName{"RAWK + ROWL!"};
constexpr WebRtcState kWebRtcState = WebRtcState::kConnectable;

// TODO(b/169550050): Implement UWBAddress.
TEST(BluetoothDeviceNameTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(kVersion, bluetooth_device_name.GetVersion());
  EXPECT_EQ(kPcp, bluetooth_device_name.GetPcp());
  EXPECT_EQ(kEndPointID, bluetooth_device_name.GetEndpointId());
  EXPECT_EQ(service_id_hash, bluetooth_device_name.GetServiceIdHash());
  EXPECT_EQ(endpoint_info, bluetooth_device_name.GetEndpointInfo());
  EXPECT_EQ(kWebRtcState, bluetooth_device_name.GetWebRtcState());
}

TEST(BluetoothDeviceNameTest, ConstructionWorksWithEmptyEndpointName) {
  ByteArray empty_endpoint_info;

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndPointID,
                                            service_id_hash,
                                            empty_endpoint_info,
                                            ByteArray{},
                                            kWebRtcState};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(kVersion, bluetooth_device_name.GetVersion());
  EXPECT_EQ(kPcp, bluetooth_device_name.GetPcp());
  EXPECT_EQ(kEndPointID, bluetooth_device_name.GetEndpointId());
  EXPECT_EQ(service_id_hash, bluetooth_device_name.GetServiceIdHash());
  EXPECT_EQ(empty_endpoint_info, bluetooth_device_name.GetEndpointInfo());
  EXPECT_EQ(kWebRtcState, bluetooth_device_name.GetWebRtcState());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BluetoothDeviceName::Version>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      bad_version,   kPcp,        kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadPcp) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      bad_pcp,     kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithShortEndpointId) {
  std::string short_endpoint_id("AB1");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        short_endpoint_id, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithLongEndpointId) {
  std::string long_endpoint_id("AB12X");

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        long_endpoint_id, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = "\x0a\x0b";

  ByteArray short_service_id_hash{short_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        kEndPointID, short_service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = "\x0a\x0b\x0c\x0d";

  ByteArray long_service_id_hash{long_service_id_hash_bytes};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        kEndPointID, long_service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};

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
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  BluetoothDeviceName bluetooth_device_name{
      kVersion,      kPcp,        kEndPointID, service_id_hash,
      endpoint_info, ByteArray{}, kWebRtcState};
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

  EXPECT_FALSE(corrupt_bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, CanParseGeneratedName) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  ByteArray endpoint_info{std::string(kEndPointName)};
  // Build name1 from scratch.
  BluetoothDeviceName name1{kVersion,        kPcp,          kEndPointID,
                            service_id_hash, endpoint_info, ByteArray{},
                            kWebRtcState};
  // Build name2 from string composed from name1.
  BluetoothDeviceName name2{std::string(name1)};
  EXPECT_TRUE(name1.IsValid());
  EXPECT_TRUE(name2.IsValid());
  EXPECT_EQ(name1.GetVersion(), name2.GetVersion());
  EXPECT_EQ(name1.GetPcp(), name2.GetPcp());
  EXPECT_EQ(name1.GetEndpointId(), name2.GetEndpointId());
  EXPECT_EQ(name1.GetServiceIdHash(), name2.GetServiceIdHash());
  EXPECT_EQ(name1.GetEndpointInfo(), name2.GetEndpointInfo());
  EXPECT_EQ(name1.GetWebRtcState(), name2.GetWebRtcState());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
