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

#include "internal/platform/implementation/windows/device_info.h"

#include <functional>
#include <string>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace windows {
namespace {

TEST(DeviceInfo, DISABLED_GetComputerName) {
  EXPECT_TRUE(DeviceInfo().GetOsDeviceName().has_value());
}

TEST(DeviceInfo, DISABLED_GetDeviceType) {
  EXPECT_EQ(DeviceInfo().GetDeviceType(), api::DeviceInfo::DeviceType::kLaptop);
}

TEST(DeviceInfo, GetOsType) {
  EXPECT_EQ(DeviceInfo().GetOsType(), api::DeviceInfo::OsType::kWindows);
}

TEST(DeviceInfo, DISABLED_GetFullName) {
  EXPECT_TRUE(DeviceInfo().GetFullName().has_value());
}

TEST(DeviceInfo, DISABLED_GetGivenName) {
  EXPECT_TRUE(DeviceInfo().GetGivenName().has_value());
}

TEST(DeviceInfo, DISABLED_GetLastName) {
  EXPECT_TRUE(DeviceInfo().GetLastName().has_value());
}

TEST(DeviceInfo, DISABLED_GetProfileUserName) {
  EXPECT_TRUE(DeviceInfo().GetProfileUserName().has_value());
}

TEST(DeviceInfo, DISABLED_GetLocalAppDataPath) {
  EXPECT_TRUE(DeviceInfo().GetLocalAppDataPath().has_value());
}

TEST(DeviceInfo, DISABLED_GetDownloadPath) {
  EXPECT_TRUE(DeviceInfo().GetDownloadPath().has_value());
}

TEST(DeviceInfo, DISABLED_GetTemporaryPath) {
  EXPECT_TRUE(DeviceInfo().GetTemporaryPath().has_value());
}

TEST(DeviceInfo, DISABLED_IsScreenLocked) {
  EXPECT_FALSE(DeviceInfo().IsScreenLocked());
}

TEST(DeviceInfo, DISABLED_RegisterScreenLockedListener) {
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_1 =
      [](api::DeviceInfo::ScreenStatus) {};
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_2 =
      [](api::DeviceInfo::ScreenStatus) {};

  DeviceInfo device_info;
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 0);

  device_info.RegisterScreenLockedListener("listener_1", listener_1);
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 1);

  device_info.RegisterScreenLockedListener("listener_2", listener_2);
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 2);
}

TEST(DeviceInfo, DISABLED_UnregisterScreenLockedListener) {
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_1 =
      [](api::DeviceInfo::ScreenStatus) {};
  std::function<void(api::DeviceInfo::ScreenStatus)> listener_2 =
      [](api::DeviceInfo::ScreenStatus) {};

  DeviceInfo device_info;
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 0);

  device_info.RegisterScreenLockedListener("listener_1", listener_1);
  device_info.RegisterScreenLockedListener("listener_2", listener_2);
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 2);

  device_info.UnregisterScreenLockedListener("listener_1");
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 1);

  device_info.UnregisterScreenLockedListener("listener_2");
  EXPECT_EQ(device_info.screen_locked_listeners_.size(), 0);
}

TEST(DeviceInfo, DISABLED_UpdateScreenLockedListener) {
  absl::Notification notification;

  api::DeviceInfo::ScreenStatus screen_locked_tracker =
      api::DeviceInfo::ScreenStatus::kUndetermined;

  std::function<void(api::DeviceInfo::ScreenStatus)> listener =
      [&screen_locked_tracker,
       &notification](api::DeviceInfo::ScreenStatus status) {
        screen_locked_tracker = api::DeviceInfo::ScreenStatus::kLocked;
        notification.Notify();
      };

  DeviceInfo device_info;
  device_info.RegisterScreenLockedListener("listener", listener);
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(5)));
  EXPECT_EQ(screen_locked_tracker, api::DeviceInfo::ScreenStatus::kLocked);
}

}  // namespace
}  // namespace windows
}  // namespace nearby
