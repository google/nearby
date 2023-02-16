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

#include "internal/platform/bluetooth_adapter.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace {

constexpr absl::string_view kMacAddress = "4C:8B:1D:CE:BA:D1";
constexpr absl::string_view kId = "AB12";

class BlePeripheralStub : public api::ble_v2::BlePeripheral {
 public:
  explicit BlePeripheralStub(absl::string_view mac_address) {
    mac_address_ = std::string(mac_address);
  }

  std::string GetAddress() const override { return mac_address_; }

 private:
  std::string mac_address_;
};

TEST(BleV2PeripheralTest, ConstructionWorks) {
  auto api_peripheral = std::make_unique<BlePeripheralStub>(kMacAddress);

  BleV2Peripheral peripheral(api_peripheral.get());

  ASSERT_TRUE(peripheral.IsValid());
  EXPECT_EQ(peripheral.GetAddress(), kMacAddress);
}

TEST(BleV2PeripheralTest, SetIdAndPsmWorks) {
  auto api_peripheral = std::make_unique<BlePeripheralStub>(kMacAddress);
  ByteArray id((std::string(kId)));
  int psm = 2;

  BleV2Peripheral peripheral(api_peripheral.get());
  peripheral.SetId(id);
  peripheral.SetPsm(psm);

  ASSERT_TRUE(peripheral.IsValid());
  EXPECT_EQ(peripheral.GetId(), id);
  EXPECT_EQ(peripheral.GetPsm(), 2);
}

TEST(BleV2PeripheralTest, CopyConstructorAndAssignmentSuccess) {
  auto api_peripheral = std::make_unique<BlePeripheralStub>(kMacAddress);
  ByteArray id((std::string(kId)));
  int psm = 2;

  BleV2Peripheral peripheral(api_peripheral.get());
  peripheral.SetId(id);
  peripheral.SetPsm(psm);

  BleV2Peripheral copy_peripheral_1(peripheral);

  ASSERT_TRUE(copy_peripheral_1.IsValid());
  EXPECT_EQ(copy_peripheral_1.GetAddress(), kMacAddress);
  EXPECT_EQ(copy_peripheral_1.GetId(), id);
  EXPECT_EQ(copy_peripheral_1.GetPsm(), 2);

  BleV2Peripheral copy_periphera1_2 = peripheral;

  ASSERT_TRUE(copy_periphera1_2.IsValid());
  EXPECT_EQ(copy_periphera1_2.GetAddress(), kMacAddress);
  EXPECT_EQ(copy_periphera1_2.GetId(), id);
  EXPECT_EQ(copy_periphera1_2.GetPsm(), 2);
}

TEST(BleV2PeripheralTest, MoveConstructorSuccess) {
  auto api_peripheral = std::make_unique<BlePeripheralStub>(kMacAddress);
  ByteArray id((std::string(kId)));
  int psm = 2;

  BleV2Peripheral peripheral(api_peripheral.get());
  peripheral.SetId(id);
  peripheral.SetPsm(psm);

  BleV2Peripheral move_peripheral(std::move(peripheral));

  ASSERT_TRUE(move_peripheral.IsValid());
  EXPECT_EQ(move_peripheral.GetAddress(), kMacAddress);
  EXPECT_EQ(move_peripheral.GetId(), id);
  EXPECT_EQ(move_peripheral.GetPsm(), 2);
}

TEST(BleV2PeripheralTest, MoveAssignmentSuccess) {
  auto api_peripheral = std::make_unique<BlePeripheralStub>(kMacAddress);
  ByteArray id((std::string(kId)));
  int psm = 2;

  BleV2Peripheral peripheral(api_peripheral.get());
  peripheral.SetId(id);
  peripheral.SetPsm(psm);

  BleV2Peripheral move_peripheral = std::move(peripheral);

  ASSERT_TRUE(move_peripheral.IsValid());
  EXPECT_EQ(move_peripheral.GetAddress(), kMacAddress);
  EXPECT_EQ(move_peripheral.GetId(), id);
  EXPECT_EQ(move_peripheral.GetPsm(), 2);
}

TEST(BluetoothAdapterTest, ConstructorDestructorWorks) {
  BluetoothAdapter adapter;
  EXPECT_TRUE(adapter.IsValid());
}

TEST(BluetoothAdapterTest, CanSetName) {
  constexpr char kAdapterName[] = "MyBtAdapter";
  BluetoothAdapter adapter;
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kEnabled);
  EXPECT_TRUE(adapter.SetName(kAdapterName));
  EXPECT_EQ(adapter.GetName(), std::string(kAdapterName));
}

TEST(BluetoothAdapterTest, CanSetStatus) {
  BluetoothAdapter adapter;
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kEnabled);
  EXPECT_TRUE(adapter.SetStatus(BluetoothAdapter::Status::kDisabled));
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kDisabled);
}

TEST(BluetoothAdapterTest, CanSetMode) {
  BluetoothAdapter adapter;
  EXPECT_TRUE(adapter.SetScanMode(BluetoothAdapter::ScanMode::kConnectable));
  EXPECT_EQ(adapter.GetScanMode(), BluetoothAdapter::ScanMode::kConnectable);
  EXPECT_TRUE(adapter.SetScanMode(
      BluetoothAdapter::ScanMode::kConnectableDiscoverable));
  EXPECT_EQ(adapter.GetScanMode(),
            BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(adapter.SetScanMode(BluetoothAdapter::ScanMode::kNone));
  EXPECT_EQ(adapter.GetScanMode(), BluetoothAdapter::ScanMode::kNone);
}

TEST(BluetoothAdapterTest, CanGetMacAddress) {
  BluetoothAdapter adapter;
  std::string bt_mac =
      BluetoothUtils::ToString(ByteArray(adapter.GetMacAddress()));
  NEARBY_LOG(INFO, "BT MAC: '%s'", bt_mac.c_str());
  EXPECT_NE(bt_mac, "");
}

}  // namespace
}  // namespace nearby
