// Copyright 2021 Google LLC
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

#include "internal/test/fake_device_info.h"

#include <array>
#include <filesystem>
#include <functional>
#include <optional>

#include "gtest/gtest.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace {

TEST(FakeDeviceInfo, DeviceName) {
  FakeDeviceInfo device_info;
  device_info.SetOsDeviceName("windows");
  EXPECT_EQ(device_info.GetOsDeviceName(), "windows");
}

TEST(FakeDeviceInfo, DeviceType) {
  FakeDeviceInfo device_info;
  device_info.SetDeviceType(api::DeviceInfo::DeviceType::kPhone);
  EXPECT_EQ(device_info.GetDeviceType(), api::DeviceInfo::DeviceType::kPhone);
}

TEST(FakeDeviceInfo, OsType) {
  FakeDeviceInfo device_info;
  device_info.SetOsType(api::DeviceInfo::OsType::kWindows);
  EXPECT_EQ(device_info.GetOsType(), api::DeviceInfo::OsType::kWindows);
}

TEST(FakeDeviceInfo, GetDownloadPath) {
  FakeDeviceInfo device_info;
  EXPECT_EQ(device_info.GetDownloadPath(),
            std::filesystem::temp_directory_path());
  device_info.SetDownloadPath(std::filesystem::temp_directory_path() / "test");
  EXPECT_EQ(device_info.GetDownloadPath(),
            std::filesystem::temp_directory_path() / "test");
}

TEST(FakeDeviceInfo, GetAppDataPath) {
  FakeDeviceInfo device_info;
  EXPECT_EQ(device_info.GetAppDataPath(),
            std::filesystem::temp_directory_path());
  device_info.SetAppDataPath(std::filesystem::temp_directory_path() / "test");
  EXPECT_EQ(device_info.GetAppDataPath(),
            std::filesystem::temp_directory_path() / "test");
}

TEST(FakeDeviceInfo, GetTemporaryPath) {
  FakeDeviceInfo device_info;
  EXPECT_EQ(device_info.GetTemporaryPath(),
            std::filesystem::temp_directory_path());
  device_info.SetTemporaryPath(std::filesystem::temp_directory_path() / "test");
  EXPECT_EQ(device_info.GetTemporaryPath(),
            std::filesystem::temp_directory_path() / "test");
}

TEST(FakeDeviceInfo, GetAvailableDiskSpaceInBytes) {
  FakeDeviceInfo device_info;
  device_info.SetDownloadPath("download");
  device_info.SetAppDataPath("appdata");
  device_info.SetTemporaryPath("temp");

  device_info.SetAvailableDiskSpaceInBytes(device_info.GetDownloadPath(), 10);
  device_info.SetAvailableDiskSpaceInBytes(device_info.GetAppDataPath(), 100);
  device_info.SetAvailableDiskSpaceInBytes(device_info.GetTemporaryPath(),
                                           1000);

  EXPECT_EQ(
      device_info.GetAvailableDiskSpaceInBytes(device_info.GetDownloadPath()),
      10);
  EXPECT_EQ(
      device_info.GetAvailableDiskSpaceInBytes(device_info.GetAppDataPath()),
      100);
  EXPECT_EQ(
      device_info.GetAvailableDiskSpaceInBytes(device_info.GetTemporaryPath()),
      1000);
}

TEST(FakeDeviceInfo, RegisterScreenLockedListener) {
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_1 =
      [](api::DeviceInfo::ScreenStatus) {};
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_2 =
      [](api::DeviceInfo::ScreenStatus) {};

  FakeDeviceInfo device_info;
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 0);

  device_info.RegisterScreenLockedListener("listener_1", listener_1);
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 1);

  device_info.RegisterScreenLockedListener("listener_2", listener_2);
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 2);
}

TEST(FakeDeviceInfo, UnregisterScreenLockedListener) {
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_1 =
      [](api::DeviceInfo::ScreenStatus) {};
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_2 =
      [](api::DeviceInfo::ScreenStatus) {};

  FakeDeviceInfo device_info;
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 0);

  device_info.RegisterScreenLockedListener("listener_1", listener_1);
  device_info.RegisterScreenLockedListener("listener_2", listener_2);
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 2);

  device_info.UnregisterScreenLockedListener("listener_1");
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 1);

  device_info.UnregisterScreenLockedListener("listener_2");
  EXPECT_EQ(device_info.GetScreenLockedListenerCount(), 0);
}

TEST(FakeDeviceInfo, UpdateScreenLockedListener) {
  api::DeviceInfo::ScreenStatus screen_locked_tracker_1 =
      api::DeviceInfo::ScreenStatus::kUndetermined;
  api::DeviceInfo::ScreenStatus screen_locked_tracker_2 =
      api::DeviceInfo::ScreenStatus::kUndetermined;

  std::function<void(api::DeviceInfo::ScreenStatus)> listener_1 =
      [&screen_locked_tracker_1](api::DeviceInfo::ScreenStatus status) {
        screen_locked_tracker_1 = status;
      };
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_2 =
      [&screen_locked_tracker_2](api::DeviceInfo::ScreenStatus status) {
        screen_locked_tracker_2 = status;
      };

  FakeDeviceInfo device_info;
  device_info.RegisterScreenLockedListener("listener_1", listener_1);
  device_info.RegisterScreenLockedListener("listener_2", listener_2);

  device_info.SetScreenLocked(true);
  EXPECT_EQ(screen_locked_tracker_1, api::DeviceInfo::ScreenStatus::kLocked);
  EXPECT_EQ(screen_locked_tracker_2, api::DeviceInfo::ScreenStatus::kLocked);

  device_info.SetScreenLocked(false);
  EXPECT_EQ(screen_locked_tracker_1, api::DeviceInfo::ScreenStatus::kUnlocked);
  EXPECT_EQ(screen_locked_tracker_2, api::DeviceInfo::ScreenStatus::kUnlocked);
}

TEST(FakeDeviceInfo, PreventSleep) {
  FakeDeviceInfo device_info;
  EXPECT_TRUE(device_info.PreventSleep());
}

TEST(FakeDeviceInfo, AllowSleep) {
  FakeDeviceInfo device_info;
  EXPECT_TRUE(device_info.AllowSleep());
}

}  // namespace
}  // namespace nearby
