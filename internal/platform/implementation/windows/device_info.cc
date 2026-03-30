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

#include <shlobj_core.h>
#include <windows.h>
#include <wtsapi32.h>

#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/files.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/windows/device_paths.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"

namespace nearby::windows {

using ::nearby::windows::string_utils::WideStringToString;

std::optional<std::string> DeviceInfo::GetOsDeviceName() const {
  std::optional<std::wstring> device_name = GetDnsHostName();
  if (device_name.has_value()) {
    return WideStringToString(*device_name);
  }
  return std::nullopt;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  return api::DeviceInfo::DeviceType::kLaptop;
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  return api::DeviceInfo::OsType::kWindows;
}

FilePath DeviceInfo::GetDownloadPath() const {
  PWSTR path;
  HRESULT result =
      SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr, &path);
  if (result == S_OK) {
    std::wstring download_path{path};
    CoTaskMemFree(path);
    return FilePath(std::wstring_view(download_path));
  }

  CoTaskMemFree(path);
  return Files::GetTemporaryDirectory();
}

FilePath DeviceInfo::GetLocalAppDataPath(FilePath sub_path) const {
  return nearby::platform::windows::GetLocalAppDataPath(sub_path);
}

FilePath DeviceInfo::GetTemporaryPath() const {
  return Files::GetTemporaryDirectory();
}

FilePath DeviceInfo::GetLogPath() const {
  return nearby::platform::windows::GetLogPath();
}

bool DeviceInfo::IsScreenLocked() const {
  absl::MutexLock lock(mutex_);
  return session_manager_.IsScreenLocked();
}

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  absl::MutexLock lock(mutex_);
  session_manager_.RegisterSessionListener(
      listener_name,
      [callback = std::move(callback)](SessionManager::SessionState state) {
        if (state == SessionManager::SessionState::kLock) {
          callback(api::DeviceInfo::ScreenStatus::kLocked);
        } else if (state == SessionManager::SessionState::kUnlock) {
          callback(api::DeviceInfo::ScreenStatus::kUnlocked);
        }
      });
}

void DeviceInfo::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(mutex_);
  session_manager_.UnregisterSessionListener(listener_name);
}

bool DeviceInfo::PreventSleep() {
  absl::MutexLock lock(mutex_);
  return session_manager_.PreventSleep();
}

bool DeviceInfo::AllowSleep() {
  absl::MutexLock lock(mutex_);
  return session_manager_.AllowSleep();
}

}  // namespace nearby::windows
