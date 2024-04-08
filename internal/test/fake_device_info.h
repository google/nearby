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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_DEVICE_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_DEVICE_INFO_H_

#include <cstddef>
#include <filesystem>  // NOLINT
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {

class FakeDeviceInfo : public DeviceInfo {
 public:
  std::string GetOsDeviceName() const override { return device_name_; }

  api::DeviceInfo::DeviceType GetDeviceType() const override {
    return device_type_;
  }

  api::DeviceInfo::OsType GetOsType() const override { return os_type_; }

  std::optional<std::string> GetFullName() const override {
    return full_name_;
  }
  std::optional<std::string> GetGivenName() const override {
    return given_name_;
  }
  std::optional<std::string> GetLastName() const override {
    return last_name_;
  }
  std::optional<std::string> GetProfileUserName() const override {
    return profile_user_name_;
  }

  std::filesystem::path GetDownloadPath() const override {
    return download_path_;
  }

  std::filesystem::path GetAppDataPath() const override {
    return app_data_path_;
  }

  std::filesystem::path GetTemporaryPath() const override { return temp_path_; }

  std::filesystem::path GetLogPath() const override { return temp_path_; }

  std::optional<size_t> GetAvailableDiskSpaceInBytes(
      const std::filesystem::path& path) const override {
    std::wstring path_key = path.wstring();
    auto it = available_space_map_.find(path_key);
    if (it != available_space_map_.end()) {
      return it->second;
    }
    return std::numeric_limits<int64_t>::max();
  }

  bool IsScreenLocked() const override { return is_screen_locked_; }

  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override {
    screen_locked_listeners_.emplace(listener_name, callback);
  }

  void UnregisterScreenLockedListener(
      absl::string_view listener_name) override {
    screen_locked_listeners_.erase(listener_name);
  }

  bool PreventSleep() override { return true; }

  bool AllowSleep() override { return true; }

  int GetScreenLockedListenerCount() { return screen_locked_listeners_.size(); }

  // Mock methods.
  void SetOsDeviceName(std::string_view device_name) {
    device_name_ = device_name;
  }

  void SetDeviceType(api::DeviceInfo::DeviceType device_type) {
    device_type_ = device_type;
  }

  void SetOsType(api::DeviceInfo::OsType os_type) { os_type_ = os_type; }

  void SetFullName(std::optional<std::string> full_name) {
    if (full_name.has_value() && !full_name->empty()) {
      full_name_ = full_name;
    } else {
      full_name_ = std::nullopt;
    }
  }

  void SetGivenName(std::optional<std::string> given_name) {
    if (given_name.has_value() && !given_name->empty()) {
      given_name_ = given_name;
    } else {
      given_name_ = std::nullopt;
    }
  }

  void SetLastName(std::optional<std::string> last_name) {
    if (last_name.has_value() && !last_name->empty()) {
      last_name_ = last_name;
    } else {
      last_name_ = std::nullopt;
    }
  }

  void SetProfileUserName(std::optional<std::string> profile_user_name) {
    if (profile_user_name.has_value() && !profile_user_name->empty()) {
      profile_user_name_ = profile_user_name;
    } else {
      profile_user_name_ = std::nullopt;
    }
  }

  void SetDownloadPath(std::filesystem::path path) { download_path_ = path; }

  void SetAppDataPath(std::filesystem::path path) { app_data_path_ = path; }

  void SetTemporaryPath(std::filesystem::path path) { temp_path_ = path; }

  void SetAvailableDiskSpaceInBytes(const std::filesystem::path& path,
                                    size_t available_bytes) {
    available_space_map_.emplace(path.wstring(), available_bytes);
  }

  void ResetDiskSpace() { available_space_map_.clear(); }

  void SetScreenLocked(bool locked) {
    is_screen_locked_ = locked;
    for (auto& listener : screen_locked_listeners_) {
      if (locked) {
        listener.second(api::DeviceInfo::ScreenStatus::kLocked);
      } else {
        listener.second(api::DeviceInfo::ScreenStatus::kUnlocked);
      }
    }
  }

 private:
  std::string device_name_ = "nearby";
  api::DeviceInfo::DeviceType device_type_ =
      api::DeviceInfo::DeviceType::kLaptop;
  api::DeviceInfo::OsType os_type_ = api::DeviceInfo::OsType::kWindows;
  std::optional<std::string> full_name_ = "Nearby";
  std::optional<std::string> given_name_ = "Nearby";
  std::optional<std::string> last_name_ = "Nearby";
  std::optional<std::string> profile_user_name_ = "nearby";
  std::filesystem::path download_path_ = std::filesystem::temp_directory_path();
  std::filesystem::path app_data_path_ = std::filesystem::temp_directory_path();
  std::filesystem::path temp_path_ = std::filesystem::temp_directory_path();
  absl::flat_hash_map<std::wstring, size_t> available_space_map_;
  absl::flat_hash_map<std::string,
                      std::function<void(api::DeviceInfo::ScreenStatus)>>
      screen_locked_listeners_;
  bool is_screen_locked_ = false;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_DEVICE_INFO_H_
