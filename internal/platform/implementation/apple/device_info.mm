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

#import "internal/platform/implementation/apple/device_info.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace apple {

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const { return u"OS Device Name"; }

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  return api::DeviceInfo::DeviceType::kLaptop;
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const { return api::DeviceInfo::OsType::kIos; }

std::optional<std::u16string> DeviceInfo::GetFullName() const { return u"Full Name"; }
std::optional<std::u16string> DeviceInfo::GetGivenName() const { return u"Given Name"; }
std::optional<std::u16string> DeviceInfo::GetLastName() const { return u"Last Name"; }
std::optional<std::string> DeviceInfo::GetProfileUserName() const { return "Username"; }

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetCommonAppDataPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  return std::filesystem::temp_directory_path();
}

bool DeviceInfo::IsScreenLocked() const { return false; }

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name, std::function<void(api::DeviceInfo::ScreenStatus)> callback) {}

void DeviceInfo::UnregisterScreenLockedListener(absl::string_view listener_name) {}

}  // namespace apple
}  // namespace nearby
