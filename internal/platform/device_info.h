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

#ifndef PLATFORM_PUBLIC_DEVICE_INFO_H_
#define PLATFORM_PUBLIC_DEVICE_INFO_H_

#include <cstddef>
#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {

class DeviceInfo {
 public:
  virtual ~DeviceInfo() = default;

  // All strings are UTF-8 encoded.
  virtual std::string GetOsDeviceName() const = 0;
  virtual api::DeviceInfo::DeviceType GetDeviceType() const = 0;
  virtual api::DeviceInfo::OsType GetOsType() const = 0;
  virtual std::optional<std::string> GetFullName() const = 0;
  virtual std::optional<std::string> GetGivenName() const = 0;
  virtual std::optional<std::string> GetLastName() const = 0;
  virtual std::optional<std::string> GetProfileUserName() const = 0;

  virtual std::filesystem::path GetDownloadPath() const = 0;
  virtual std::filesystem::path GetAppDataPath() const = 0;
  virtual std::filesystem::path GetTemporaryPath() const = 0;
  virtual std::filesystem::path GetLogPath() const = 0;

  virtual std::optional<size_t> GetAvailableDiskSpaceInBytes(
      const std::filesystem::path& path) const = 0;

  virtual bool IsScreenLocked() const = 0;
  virtual void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) = 0;
  virtual void UnregisterScreenLockedListener(
      absl::string_view listener_name) = 0;

  virtual bool PreventSleep() = 0;
  virtual bool AllowSleep() = 0;

  // Returns UTF-8 encoded localized device name depending on device type.
  std::string GetDeviceTypeName() const {
    // TODO(b/230132370): return localized device name.
    switch (GetDeviceType()) {
      case api::DeviceInfo::DeviceType::kPhone:
        return "Phone";
      case api::DeviceInfo::DeviceType::kTablet:
        return "Tablet";
      case api::DeviceInfo::DeviceType::kLaptop:
        return "PC";
      default:
        return "Unknown";
    }
  }
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_DEVICE_INFO_H_
