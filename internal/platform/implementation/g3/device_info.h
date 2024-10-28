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

#ifndef PLATFORM_IMPL_G3_DEVICE_INFO_H_
#define PLATFORM_IMPL_G3_DEVICE_INFO_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace g3 {

class DeviceInfo : public api::DeviceInfo {
 public:
  std::optional<std::string> GetOsDeviceName() const override {
    return "Windows";
  }

  api::DeviceInfo::DeviceType GetDeviceType() const override {
    return api::DeviceInfo::DeviceType::kLaptop;
  }

  api::DeviceInfo::OsType GetOsType() const override {
    return api::DeviceInfo::OsType::kChromeOs;
  }

  std::optional<std::filesystem::path> GetDownloadPath() const override {
    return std::filesystem::temp_directory_path();
  }

  std::optional<std::filesystem::path> GetLocalAppDataPath() const override {
    return std::filesystem::temp_directory_path();
  }

  std::optional<std::filesystem::path> GetCommonAppDataPath() const override {
    return std::filesystem::temp_directory_path();
  }

  std::optional<std::filesystem::path> GetTemporaryPath() const override {
    return std::filesystem::temp_directory_path();
  }

  std::optional<std::filesystem::path> GetLogPath() const override {
    return std::filesystem::temp_directory_path();
  }

  std::optional<std::filesystem::path> GetCrashDumpPath() const override {
    return std::filesystem::temp_directory_path();
  }

  bool IsScreenLocked() const override { return false; }

  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override {
    screen_locked_listeners_.emplace(listener_name, std::move(callback));
  }

  void UnregisterScreenLockedListener(
      absl::string_view listener_name) override {
    screen_locked_listeners_.erase(listener_name);
  }

  bool PreventSleep() override { return true; }

  bool AllowSleep() override { return true; }

 private:
  absl::flat_hash_map<std::string,
                      std::function<void(api::DeviceInfo::ScreenStatus)>>
      screen_locked_listeners_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_DEVICE_INFO_H_
