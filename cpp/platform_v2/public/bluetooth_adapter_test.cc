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

#include "platform_v2/public/bluetooth_adapter.h"

#include "platform_v2/base/bluetooth_utils.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

TEST(BluetoothAdapterTest, ConstructorDestructorWorks) {
  BluetoothAdapter adapter;
  EXPECT_TRUE(adapter.IsValid());
}

TEST(BluetoothAdapterTest, CanSetName) {
  constexpr char kAdapterName[] = "MyBtAdapter";
  BluetoothAdapter adapter;
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kDisabled);
  EXPECT_TRUE(adapter.SetName(kAdapterName));
  EXPECT_EQ(adapter.GetName(), std::string(kAdapterName));
}

TEST(BluetoothAdapterTest, CanSetStatus) {
  BluetoothAdapter adapter;
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kDisabled);
  EXPECT_TRUE(adapter.SetStatus(BluetoothAdapter::Status::kEnabled));
  EXPECT_EQ(adapter.GetStatus(), BluetoothAdapter::Status::kEnabled);
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
}  // namespace location
