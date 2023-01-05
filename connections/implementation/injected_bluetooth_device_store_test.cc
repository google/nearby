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

#include "connections/implementation/injected_bluetooth_device_store.h"

#include <array>

#include "gtest/gtest.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/bluetooth_adapter.h"

namespace nearby {
namespace connections {
namespace {

// Need to wrap with static_cast<char> to silence -Wc++11-narrowing issue.
constexpr std::array<char, 6> kTestRemoteBluetoothMacAddress{
    0x01, 0x23, 0x45, 0x67, static_cast<char>(0x89), static_cast<char>(0xab)};
constexpr std::array<char, 2> kTestEndpointInfo{static_cast<char>(0xcd),
                                                static_cast<char>(0xef)};
constexpr std::array<char, 3> kTestServiceIdHash{0x01, 0x23, 0x45};

const char kTestEndpointId[] = "abcd";

class InjectedBluetoothDeviceStoreTest : public testing::Test {
 protected:
  InjectedBluetoothDeviceStore store_;
};

TEST_F(InjectedBluetoothDeviceStoreTest, Success) {
  ByteArray remote_bluetooth_mac_address(kTestRemoteBluetoothMacAddress);
  ByteArray endpoint_info(kTestEndpointInfo);
  ByteArray service_id_hash(kTestServiceIdHash);

  BluetoothDevice device = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, kTestEndpointId, endpoint_info,
      service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_TRUE(device.IsValid());

  EXPECT_EQ(BluetoothUtils::ToString(remote_bluetooth_mac_address),
            device.GetMacAddress());

  BluetoothDeviceName name(device.GetName());
  EXPECT_TRUE(name.IsValid());
  EXPECT_EQ(kTestEndpointId, name.GetEndpointId());
  EXPECT_EQ(endpoint_info, name.GetEndpointInfo());
  EXPECT_EQ(service_id_hash, name.GetServiceIdHash());
  EXPECT_EQ(Pcp::kP2pPointToPoint, name.GetPcp());
}

TEST_F(InjectedBluetoothDeviceStoreTest, Fail_InvalidBluetoothMac) {
  // Use address with only 1 byte.
  ByteArray remote_bluetooth_mac_address(std::array<char, 1>{0x00});
  ByteArray endpoint_info(kTestEndpointInfo);
  ByteArray service_id_hash(kTestServiceIdHash);

  BluetoothDevice device = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, kTestEndpointId, endpoint_info,
      service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_FALSE(device.IsValid());
}

TEST_F(InjectedBluetoothDeviceStoreTest, Fail_InvalidEndpointId) {
  ByteArray remote_bluetooth_mac_address(kTestRemoteBluetoothMacAddress);
  ByteArray endpoint_info(kTestEndpointInfo);
  ByteArray service_id_hash(kTestServiceIdHash);

  // Use empty endpoint ID.
  BluetoothDevice device1 = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, /*endpoint_id=*/std::string(),
      endpoint_info, service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_FALSE(device1.IsValid());

  // Use endpoint ID of wrong length.
  const std::string too_long_endpoint_id = "abcde";
  BluetoothDevice device2 = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, too_long_endpoint_id, endpoint_info,
      service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_FALSE(device2.IsValid());
}

TEST_F(InjectedBluetoothDeviceStoreTest, Fail_EmptyEndpointInfo) {
  ByteArray remote_bluetooth_mac_address(kTestRemoteBluetoothMacAddress);
  // Use empty endpoint info.
  ByteArray endpoint_info;
  ByteArray service_id_hash(kTestServiceIdHash);

  BluetoothDevice device = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, kTestEndpointId, endpoint_info,
      service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_FALSE(device.IsValid());
}

TEST_F(InjectedBluetoothDeviceStoreTest, Fail_InvalidServiceIdHash) {
  ByteArray remote_bluetooth_mac_address(kTestRemoteBluetoothMacAddress);
  ByteArray endpoint_info(kTestEndpointInfo);
  // Use address with only 1 byte.
  ByteArray service_id_hash(std::array<char, 1>{0x00});

  BluetoothDevice device = store_.CreateInjectedBluetoothDevice(
      remote_bluetooth_mac_address, kTestEndpointId, endpoint_info,
      service_id_hash, Pcp::kP2pPointToPoint);
  EXPECT_FALSE(device.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
