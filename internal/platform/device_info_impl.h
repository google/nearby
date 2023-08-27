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

#ifndef PLATFORM_PUBLIC_DEVICE_INFO_IMPL_H_
#define PLATFORM_PUBLIC_DEVICE_INFO_IMPL_H_

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "internal/platform/device_info.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

class DeviceInfoImpl : public DeviceInfo {
 public:
  DeviceInfoImpl()
      : device_info_impl_(api::ImplementationPlatform::CreateDeviceInfo()) {}

  std::u16string GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override;

  std::optional<std::u16string> GetFullName() const override;
  std::optional<std::u16string> GetGivenName() const override;
  std::optional<std::u16string> GetLastName() const override;
  std::optional<std::string> GetProfileUserName() const override;

  std::filesystem::path GetDownloadPath() const override;
  std::filesystem::path GetAppDataPath() const override;
  std::filesystem::path GetTemporaryPath() const override;

  std::optional<size_t> GetAvailableDiskSpaceInBytes(
      const std::filesystem::path& path) const override;

  bool IsScreenLocked() const override;
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override;
  void UnregisterScreenLockedListener(absl::string_view listener_name) override;

  bool PreventSleep() override;
  bool AllowSleep() override;

 private:
  std::unique_ptr<api::DeviceInfo> device_info_impl_;
};
}  // namespace nearby

#endif  // PLATFORM_PUBLIC_DEVICE_INFO_IMPL_H_
