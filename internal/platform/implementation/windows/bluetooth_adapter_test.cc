// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/windows/bluetooth_adapter.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

namespace nearby {
namespace windows {
namespace {

constexpr absl::string_view kName = "Test Radio Name";
constexpr absl::string_view kLongName =
    "12345678901234567890123456789012345678";
constexpr absl::string_view kSpecialCharName = "~!@#$%^&*()_+<>:\"-}[{]";

TEST(BluetoothAdapter, DISABLED_SetNameSucceeds) {
  BluetoothAdapter bluetooth_adapter;
  auto original_name = bluetooth_adapter.GetName();
  bluetooth_adapter.SetName(kName,
                            /* persist= */ false);

  bluetooth_adapter.RestoreRadioNameIfNecessary();

  EXPECT_EQ(bluetooth_adapter.GetName(), original_name);
}

TEST(BluetoothAdapter, DISABLED_SetNameWithTooManyChars) {
  BluetoothAdapter bluetooth_adapter;
  auto original_name = bluetooth_adapter.GetName();
  bluetooth_adapter.SetName(kLongName,
                            /* persist= */ false);

  auto result = bluetooth_adapter.GetName();
  bluetooth_adapter.RestoreRadioNameIfNecessary();

  EXPECT_EQ(result, kLongName);
  EXPECT_EQ(bluetooth_adapter.GetName(), original_name);
}

TEST(BluetoothAdapter, DISABLED_SetNameWithSpecialCharsSucceeds) {
  BluetoothAdapter bluetooth_adapter;
  auto original_name = bluetooth_adapter.GetName();
  bluetooth_adapter.SetName(kSpecialCharName,
                            /* persist= */ false);

  auto result = bluetooth_adapter.GetName();
  bluetooth_adapter.RestoreRadioNameIfNecessary();

  EXPECT_EQ(result, kSpecialCharName);
  EXPECT_EQ(bluetooth_adapter.GetName(), original_name);
}

TEST(BluetoothAdapter, DISABLED_SetNameWithEmptyStringSetsOriginalName) {
  BluetoothAdapter bluetooth_adapter;
  auto original_name = bluetooth_adapter.GetName();

  bluetooth_adapter.SetName("",
                            /* persist= */ false);

  EXPECT_EQ(bluetooth_adapter.GetName(), original_name);
}

TEST(BluetoothAdapter, DISABLED_SetNameWithNullPtrSetsOriginalName) {
  BluetoothAdapter bluetooth_adapter;
  auto original_name = bluetooth_adapter.GetName();

  bluetooth_adapter.SetName(nullptr,
                            /* persist= */ false);

  EXPECT_EQ(bluetooth_adapter.GetName(), original_name);
}

TEST(BluetoothAdapter, DISABLED_SetStatus) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(
      bluetooth_adapter.SetStatus(api::BluetoothAdapter::Status::kEnabled));
}

TEST(BluetoothAdapter, DISABLED_IsEnabled) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.IsEnabled());
}

TEST(BluetoothAdapter, DISABLED_GetScanMode) {
  BluetoothAdapter bluetooth_adapter;
  api::BluetoothAdapter::ScanMode scan_mode = bluetooth_adapter.GetScanMode();
  EXPECT_EQ(scan_mode, api::BluetoothAdapter::ScanMode::kNone);
}

TEST(BluetoothAdapter, DISABLED_SetScanMode) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.SetScanMode(
      api::BluetoothAdapter::ScanMode::kConnectableDiscoverable));
  api::BluetoothAdapter::ScanMode scan_mode = bluetooth_adapter.GetScanMode();
  EXPECT_EQ(scan_mode,
            api::BluetoothAdapter::ScanMode::kConnectableDiscoverable);
}

TEST(BluetoothAdapter, DISABLED_GetName) {
  BluetoothAdapter bluetooth_adapter;
  bluetooth_adapter.SetName("test");
  EXPECT_EQ(bluetooth_adapter.GetName(), "test");
}

TEST(BluetoothAdapter, DISABLED_SetName) {
  BluetoothAdapter bluetooth_adapter;
  std::string bluetooth_device_name =
      "abcdefghijklmnopqrstuvwxyz"  // 26
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 52
      "abcdefghijklmnopqrstuvwxyz"  // 78
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 104
      "abcdefghijklmnopqrstuvwxyz"  // 130
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 156
      "abcdefghijklmnopqrstuvwxyz"  // 182
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 208
      "abcdefghijklmnopqrstuvwxyz"  // 234
      "ABCDEFGHIJKLMN";             // 248
  EXPECT_TRUE(bluetooth_adapter.SetName(bluetooth_device_name));
  EXPECT_EQ(bluetooth_adapter.GetName(), bluetooth_device_name);
}

TEST(BluetoothAdapter, DISABLED_SetName_Exceeded) {
  BluetoothAdapter bluetooth_adapter;
  std::string original_bluetooth_device_name = bluetooth_adapter.GetName();
  std::string bluetooth_device_name =
      "abcdefghijklmnopqrstuvwxyz"  // 26
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 52
      "abcdefghijklmnopqrstuvwxyz"  // 78
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 104
      "abcdefghijklmnopqrstuvwxyz"  // 130
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 156
      "abcdefghijklmnopqrstuvwxyz"  // 182
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // 208
      "abcdefghijklmnopqrstuvwxyz"  // 234
      "ABCDEFGHIJKLMNO";            // 249
  EXPECT_FALSE(bluetooth_adapter.SetName(bluetooth_device_name));
  EXPECT_EQ(bluetooth_adapter.GetName(), original_bluetooth_device_name);
}

TEST(BluetoothAdapter, DISABLED_GetMacAddress) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(!bluetooth_adapter.GetMacAddress().empty());
}

TEST(BluetoothAdapter, DISABLED_SetOnScanModeChanged) {
  api::BluetoothAdapter::ScanMode mode =
      api::BluetoothAdapter::ScanMode::kUnknown;
  absl::Notification scan_mode_changed_notification;
  BluetoothAdapter bluetooth_adapter;
  bluetooth_adapter.SetScanMode(mode);

  bluetooth_adapter.SetOnScanModeChanged(
      [&](api::BluetoothAdapter::ScanMode scan_mode) {
        mode = scan_mode;
        scan_mode_changed_notification.Notify();
      });
  bluetooth_adapter.SetScanMode(
      api::BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  EXPECT_TRUE(scan_mode_changed_notification.WaitForNotificationWithTimeout(
      absl::Seconds(5)));
  EXPECT_EQ(mode, api::BluetoothAdapter::ScanMode::kConnectableDiscoverable);
}

TEST(BluetoothAdapter, DISABLED_IsExtendedAdvertisingSupported) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.IsExtendedAdvertisingSupported());
}

TEST(BluetoothAdapter, DISABLED_IsCentralRoleSupported) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.IsCentralRoleSupported());
}

TEST(BluetoothAdapter, DISABLED_IsPeripheralRoleSupported) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.IsPeripheralRoleSupported());
}

TEST(BluetoothAdapter, DISABLED_IsLowEnergySupported) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(bluetooth_adapter.IsLowEnergySupported());
}

TEST(BluetoothAdapter, DISABLED_GetNameFromComputerName) {
  BluetoothAdapter bluetooth_adapter;
  EXPECT_TRUE(!bluetooth_adapter.GetNameFromComputerName().empty());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
