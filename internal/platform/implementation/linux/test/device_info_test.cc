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

#include "internal/platform/implementation/linux/device_info.h"

#include <cstring>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/dbus.h"

namespace nearby {
namespace linux {
namespace {

class DeviceInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    system_bus_ = getSystemBusConnection();
    device_info_ = std::make_unique<DeviceInfo>(system_bus_);
  }

  void TearDown() override {
    device_info_.reset();
  }

  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::unique_ptr<DeviceInfo> device_info_;
};

TEST_F(DeviceInfoTest, GetComputerName) {
  ASSERT_TRUE(device_info_->GetOsDeviceName().has_value());
  std::string device_name = device_info_->GetOsDeviceName().value();
  // Makes sure device_name does not include terminating null character.
  EXPECT_EQ(device_name.size(), std::strlen(device_name.data()));
}

TEST_F(DeviceInfoTest, DISABLED_GetDeviceType) {
  EXPECT_EQ(device_info_->GetDeviceType(), api::DeviceInfo::DeviceType::kLaptop);
}

TEST_F(DeviceInfoTest, GetOsType) {
  EXPECT_EQ(device_info_->GetOsType(), api::DeviceInfo::OsType::kWindows);
}

TEST_F(DeviceInfoTest, DISABLED_GetLocalAppDataPath) {
  EXPECT_TRUE(device_info_->GetLocalAppDataPath().has_value());
}

TEST_F(DeviceInfoTest, DISABLED_GetDownloadPath) {
  EXPECT_TRUE(device_info_->GetDownloadPath().has_value());
}

TEST_F(DeviceInfoTest, DISABLED_GetTemporaryPath) {
  EXPECT_TRUE(device_info_->GetTemporaryPath().has_value());
}

TEST_F(DeviceInfoTest, DISABLED_GetLogPath) {
  EXPECT_TRUE(device_info_->GetLogPath().has_value());
}

TEST_F(DeviceInfoTest, DISABLED_GetCrashDumpPath) {
  EXPECT_TRUE(device_info_->GetCrashDumpPath().has_value());
}

TEST_F(DeviceInfoTest, DISABLED_IsScreenLocked) {
  EXPECT_FALSE(device_info_->IsScreenLocked());
}

TEST_F(DeviceInfoTest, DISABLED_PreventSleep) {
  EXPECT_TRUE(device_info_->PreventSleep());
}

TEST_F(DeviceInfoTest, DISABLED_AllowSleep) {
  EXPECT_TRUE(device_info_->AllowSleep());
}

}  // namespace
}  // namespace linux
}  // namespace nearby
