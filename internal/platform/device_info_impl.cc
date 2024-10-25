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

#include "internal/platform/device_info_impl.h"

#include <cstddef>
#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>
#include "absl/strings/string_view.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {

std::string DeviceInfoImpl::GetOsDeviceName() const {
  std::optional<std::string> device_name =
      device_info_impl_->GetOsDeviceName();
  if (device_name.has_value()) {
    return *device_name;
  }

  return "unknown";
}

api::DeviceInfo::DeviceType DeviceInfoImpl::GetDeviceType() const {
  return device_info_impl_->GetDeviceType();
}

api::DeviceInfo::OsType DeviceInfoImpl::GetOsType() const {
  return device_info_impl_->GetOsType();
}

std::optional<std::string> DeviceInfoImpl::GetGivenName() const {
  return device_info_impl_->GetGivenName();
}

std::filesystem::path DeviceInfoImpl::GetDownloadPath() const {
  std::optional<std::filesystem::path> path =
      device_info_impl_->GetDownloadPath();
  if (path.has_value()) {
    return *path;
  }
  return nearby::sharing::GetTemporaryDirectory().value_or(
      nearby::sharing::CurrentDirectory());
}

std::filesystem::path DeviceInfoImpl::GetAppDataPath() const {
  std::optional<std::filesystem::path> path =
      device_info_impl_->GetLocalAppDataPath();
  if (path.has_value()) {
    return *path;
  }
  return nearby::sharing::GetTemporaryDirectory().value_or(
      nearby::sharing::CurrentDirectory());
}

std::filesystem::path DeviceInfoImpl::GetTemporaryPath() const {
  std::optional<std::filesystem::path> path =
      device_info_impl_->GetTemporaryPath();
  if (path.has_value()) {
    return *path;
  }
  return nearby::sharing::GetTemporaryDirectory().value_or(
      nearby::sharing::CurrentDirectory());
}

std::filesystem::path DeviceInfoImpl::GetLogPath() const {
  std::optional<std::filesystem::path> path = device_info_impl_->GetLogPath();
  return path.value_or(GetTemporaryPath());
}

std::optional<size_t> DeviceInfoImpl::GetAvailableDiskSpaceInBytes(
    const std::filesystem::path& path) const {
  std::error_code error_code;
  std::filesystem::space_info space_info =
      std::filesystem::space(path, error_code);
  if (error_code.value() == 0) {
    return space_info.available;
  }
  return std::nullopt;
}

bool DeviceInfoImpl::IsScreenLocked() const {
  return device_info_impl_->IsScreenLocked();
}

void DeviceInfoImpl::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  device_info_impl_->RegisterScreenLockedListener(listener_name, callback);
}

void DeviceInfoImpl::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  device_info_impl_->UnregisterScreenLockedListener(listener_name);
}

bool DeviceInfoImpl::PreventSleep() {
  return device_info_impl_->PreventSleep();
}

bool DeviceInfoImpl::AllowSleep() { return device_info_impl_->AllowSleep(); }

}  // namespace nearby
