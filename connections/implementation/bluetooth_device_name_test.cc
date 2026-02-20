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

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/webrtc_state.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace {

constexpr BluetoothDeviceName::Version kVersion =
    BluetoothDeviceName::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kEndpointId = "ABCD";
constexpr absl::string_view kServiceIdHash = "ABC";
constexpr absl::string_view kEndpointInfo = "GG";
constexpr WebRtcState kWebRtcState = WebRtcState::kConnectable;
constexpr int kMaxEndpointInfoLength = 131;

TEST(BluetoothDeviceNameTest, ConstructionWithUwbAddress) {
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  ByteArray uwb_address{{0x01, 0x02}};
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(bluetooth_device_name.GetVersion(), kVersion);
  EXPECT_EQ(bluetooth_device_name.GetPcp(), kPcp);
  EXPECT_EQ(bluetooth_device_name.GetEndpointId(), kEndpointId);
  EXPECT_EQ(bluetooth_device_name.GetServiceIdHash(), service_id_hash);
  EXPECT_EQ(bluetooth_device_name.GetEndpointInfo(), endpoint_info);
  EXPECT_EQ(bluetooth_device_name.GetUwbAddress(), uwb_address);
  EXPECT_EQ(bluetooth_device_name.GetWebRtcState(), kWebRtcState);
}

TEST(BluetoothDeviceNameTest, DeserializationWithUwbAddress) {
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  ByteArray uwb_address{{0x01, 0x02}};
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};

  std::string bluetooth_device_name_string = std::string(bluetooth_device_name);

  BluetoothDeviceName bluetooth_device_name_from_string(
      bluetooth_device_name_string);
  EXPECT_TRUE(bluetooth_device_name_from_string.IsValid());
  EXPECT_EQ(bluetooth_device_name_from_string.GetVersion(), kVersion);
  EXPECT_EQ(bluetooth_device_name_from_string.GetPcp(), kPcp);
  EXPECT_EQ(bluetooth_device_name_from_string.GetEndpointId(), kEndpointId);
  EXPECT_EQ(bluetooth_device_name_from_string.GetServiceIdHash(),
            service_id_hash);
  EXPECT_EQ(bluetooth_device_name_from_string.GetEndpointInfo(), endpoint_info);
  EXPECT_EQ(bluetooth_device_name_from_string.GetUwbAddress(), uwb_address);
  EXPECT_EQ(bluetooth_device_name_from_string.GetWebRtcState(), kWebRtcState);
}

TEST(BluetoothDeviceNameTest,
     ConstructionWithEmptyUwbAddressAndEmptyEndpointName) {
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray empty_endpoint_info;
  ByteArray uwb_address;
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            empty_endpoint_info,
                                            uwb_address,
                                            kWebRtcState};

  EXPECT_TRUE(bluetooth_device_name.IsValid());
  EXPECT_EQ(bluetooth_device_name.GetVersion(), kVersion);
  EXPECT_EQ(bluetooth_device_name.GetPcp(), kPcp);
  EXPECT_EQ(bluetooth_device_name.GetEndpointId(), kEndpointId);
  EXPECT_EQ(bluetooth_device_name.GetServiceIdHash(), service_id_hash);
  EXPECT_TRUE(bluetooth_device_name.GetEndpointInfo().Empty());
  EXPECT_TRUE(bluetooth_device_name.GetUwbAddress().Empty());
  EXPECT_EQ(bluetooth_device_name.GetWebRtcState(), kWebRtcState);
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BluetoothDeviceName::Version>(666);
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  ByteArray uwb_address;
  BluetoothDeviceName bluetooth_device_name{bad_version,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};
  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadPcp) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            bad_pcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            ByteArray{},
                                            kWebRtcState};

  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadEndpointIdLength) {
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  ByteArray uwb_address;
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            "1",
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};
  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithBadServiceIdHashLength) {
  ByteArray service_id_hash{"12"};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  ByteArray uwb_address;
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};
  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithBadInput) {
  BluetoothDeviceName bluetooth_device_name{"bad input"};
  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithShortInput) {
  BluetoothDeviceName bluetooth_device_name{
      Base64Utils::Encode(ByteArray{"1"})};
  EXPECT_FALSE(bluetooth_device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, EndpointInfoTruncation) {
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  std::string long_endpoint_info_string(150, 'a');
  ByteArray endpoint_info{long_endpoint_info_string};
  ByteArray uwb_address;
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            uwb_address,
                                            kWebRtcState};
  EXPECT_EQ(bluetooth_device_name.GetEndpointInfo().size(),
            kMaxEndpointInfoLength);

  std::string bluetooth_device_name_string = std::string(bluetooth_device_name);

  BluetoothDeviceName bluetooth_device_name_from_string(
      bluetooth_device_name_string);
  EXPECT_TRUE(bluetooth_device_name_from_string.IsValid());
  EXPECT_EQ(bluetooth_device_name_from_string.GetEndpointInfo(),
            bluetooth_device_name.GetEndpointInfo());
  EXPECT_EQ(bluetooth_device_name_from_string.GetEndpointInfo().size(),
            kMaxEndpointInfoLength);
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithBadVersion) {
  // version=7, pcp=1
  ByteArray bytes(
      "\xE1"
      "234567890123456",
      16);
  BluetoothDeviceName device_name(Base64Utils::Encode(bytes));
  EXPECT_FALSE(device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, DeserializationFailsWithBadPcp) {
  // version=1, pcp=31
  ByteArray bytes(
      "\x3F"
      "234567890123456",
      16);
  BluetoothDeviceName device_name(Base64Utils::Encode(bytes));
  EXPECT_FALSE(device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, InvalidToString) {
  BluetoothDeviceName device_name;
  EXPECT_TRUE(std::string(device_name).empty());
  EXPECT_FALSE(device_name.IsValid());
}

TEST(BluetoothDeviceNameTest, ConstructionFailsWithWrongEndpointNameLength) {
  // Serialize good data into a good Bluetooth Device Name.
  ByteArray service_id_hash{std::string(kServiceIdHash)};
  ByteArray endpoint_info{std::string(kEndpointInfo)};
  BluetoothDeviceName bluetooth_device_name{kVersion,
                                            kPcp,
                                            kEndpointId,
                                            service_id_hash,
                                            endpoint_info,
                                            ByteArray{},
                                            kWebRtcState};
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
}  // namespace
}  // namespace connections
}  // namespace nearby
