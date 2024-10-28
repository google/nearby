// Copyright 2021-2023 Google LLC
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

#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace windows {
namespace {

TEST(DeviceInfo, GetComputerName) {
  ASSERT_TRUE(DeviceInfo().GetOsDeviceName().has_value());
  std::string device_name = DeviceInfo().GetOsDeviceName().value();
  // Makes sure device_name does not include terminating null character.
  EXPECT_EQ(device_name.size(), std::strlen(device_name.data()));
}

TEST(DeviceInfo, DISABLED_GetDeviceType) {
  EXPECT_EQ(DeviceInfo().GetDeviceType(), api::DeviceInfo::DeviceType::kLaptop);
}

TEST(DeviceInfo, GetOsType) {
  EXPECT_EQ(DeviceInfo().GetOsType(), api::DeviceInfo::OsType::kWindows);
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

TEST(DeviceInfo, DISABLED_GetLogPath) {
  EXPECT_TRUE(DeviceInfo().GetLogPath().has_value());
}

TEST(DeviceInfo, DISABLED_GetCrashDumpPath) {
  EXPECT_TRUE(DeviceInfo().GetCrashDumpPath().has_value());
}

TEST(DeviceInfo, DISABLED_IsScreenLocked) {
  EXPECT_FALSE(DeviceInfo().IsScreenLocked());
}

TEST(DeviceInfo, DISABLED_PreventSleep) {
  EXPECT_TRUE(DeviceInfo().PreventSleep());
}

TEST(DeviceInfo, DISABLED_AllowSleep) {
  EXPECT_TRUE(DeviceInfo().AllowSleep());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
