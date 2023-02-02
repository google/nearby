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

#ifndef PLATFORM_API_DEVICE_INFO_H_
#define PLATFORM_API_DEVICE_INFO_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace nearby {
namespace api {

class DeviceInfo {
 public:
  enum class ScreenStatus { kUndetermined = 0, kLocked, kUnlocked };
  enum class DeviceType { kUnknown = 0, kPhone, kTablet, kLaptop };
  enum class OsType { kUnknown = 0, kAndroid, kChromeOs, kIos, kWindows };

  virtual ~DeviceInfo() = default;

  // Gets device name.
  virtual std::optional<std::u16string> GetOsDeviceName() const = 0;
  virtual DeviceType GetDeviceType() const = 0;
  virtual OsType GetOsType() const = 0;

  // Gets basic information of current user.
  virtual std::optional<std::u16string> GetFullName() const = 0;
  virtual std::optional<std::u16string> GetGivenName() const = 0;
  virtual std::optional<std::u16string> GetLastName() const = 0;
  virtual std::optional<std::string> GetProfileUserName() const = 0;

  // Gets known paths of current user.
  virtual std::optional<std::filesystem::path> GetDownloadPath() const = 0;
  virtual std::optional<std::filesystem::path> GetLocalAppDataPath() const = 0;
  virtual std::optional<std::filesystem::path> GetCommonAppDataPath() const = 0;
  virtual std::optional<std::filesystem::path> GetTemporaryPath() const = 0;
  virtual std::optional<std::filesystem::path> GetLogPath() const = 0;
  virtual std::optional<std::filesystem::path> GetCrashDumpPath() const = 0;

  // Monitor screen status
  virtual bool IsScreenLocked() const = 0;
  virtual void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(ScreenStatus)> callback) = 0;
  virtual void UnregisterScreenLockedListener(
      absl::string_view listener_name) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_DEVICE_INFO_H_
