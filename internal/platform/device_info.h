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

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

class DeviceInfo {
 public:
  virtual ~DeviceInfo() = default;

  virtual std::u16string GetOsDeviceName() const = 0;
  virtual api::DeviceInfo::DeviceType GetDeviceType() const = 0;
  virtual api::DeviceInfo::OsType GetOsType() const = 0;
  virtual std::optional<std::u16string> GetFullName() const = 0;
  virtual std::optional<std::u16string> GetGivenName() const = 0;
  virtual std::optional<std::u16string> GetLastName() const = 0;
  virtual std::optional<std::string> GetProfileUserName() const = 0;

  virtual std::filesystem::path GetDownloadPath() const = 0;
  virtual std::filesystem::path GetAppDataPath() const = 0;
  virtual std::filesystem::path GetTemporaryPath() const = 0;

  virtual std::optional<size_t> GetAvailableDiskSpaceInBytes(
      const std::filesystem::path& path) const = 0;

  virtual bool IsScreenLocked() const = 0;
  virtual void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) = 0;
  virtual void UnregisterScreenLockedListener(
      absl::string_view listener_name) = 0;

  // Returns localized device name depends on device type.
  std::u16string GetDeviceTypeName() const {
    // TODO(b/230132370): return localized device name.
    switch (GetDeviceType()) {
      case api::DeviceInfo::DeviceType::kPhone:
        return u"Phone";
      case api::DeviceInfo::DeviceType::kTablet:
        return u"Tablet";
      case api::DeviceInfo::DeviceType::kLaptop:
        return u"PC";
      default:
        return u"Unknown";
    }
  }
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_DEVICE_INFO_H_
