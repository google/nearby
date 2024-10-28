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

#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.System.h"

namespace nearby {
namespace windows {

using IInspectable = winrt::Windows::Foundation::IInspectable;
using KnownUserProperties = winrt::Windows::System::KnownUserProperties;
using User = winrt::Windows::System::User;
using UserType = winrt::Windows::System::UserType;
using UserAuthenticationStatus =
    winrt::Windows::System::UserAuthenticationStatus;

using ::nearby::windows::string_utils::WideStringToString;

template <typename T>
using IVectorView = winrt::Windows::Foundation::Collections::IVectorView<T>;

template <typename T>
using IAsyncOperation = winrt::Windows::Foundation::IAsyncOperation<T>;

constexpr char logs_relative_path[] = "Google\\Nearby\\Sharing\\Logs";
constexpr char crash_dumps_relative_path[] =
    "Google\\Nearby\\Sharing\\CrashDumps";

std::optional<std::string> DeviceInfo::GetOsDeviceName() const {
  DWORD size = 0;

  // Get length of the computer name.
  if (GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size) == 0) {
    if (GetLastError() != ERROR_MORE_DATA) {
      LOG(ERROR) << ": Failed to get device name size, error:"
                 << GetLastError();
      return std::nullopt;
    }
  }
  std::wstring device_name(size, L' ');
  if (GetComputerNameExW(ComputerNameDnsHostname, device_name.data(), &size) !=
      0) {
    // On input size includes null termination.
    // On output size excludes null termination.
    device_name.resize(size);
    return WideStringToString(device_name);
  }

  LOG(ERROR) << ": Failed to get device name, error:" << GetLastError();
  return std::nullopt;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  // TODO(b/230132370): return correct device type on the Windows platform.
  return api::DeviceInfo::DeviceType::kLaptop;
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  return api::DeviceInfo::OsType::kWindows;
}

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  PWSTR path;
  HRESULT result =
      SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr, &path);
  if (result == S_OK) {
    std::wstring download_path{path};
    CoTaskMemFree(path);
    return std::filesystem::path(download_path);
  }

  CoTaskMemFree(path);
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  PWSTR path;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    std::wstring local_appdata_path{path};
    CoTaskMemFree(path);
    return std::filesystem::path(local_appdata_path);
  }

  CoTaskMemFree(path);
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCommonAppDataPath() const {
  PWSTR path;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    std::wstring common_app_data_path{path};
    CoTaskMemFree(path);
    return std::filesystem::path(common_app_data_path);
  }

  CoTaskMemFree(path);
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  return nearby::sharing::GetTemporaryDirectory();
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  auto prefix_path = GetLocalAppDataPath();
  if (prefix_path.has_value()) {
    return std::filesystem::path(prefix_path.value() / logs_relative_path);
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  auto prefix_path = GetLocalAppDataPath();
  if (prefix_path.has_value()) {
    return std::filesystem::path(prefix_path.value() /
                                 crash_dumps_relative_path);
  }
  return std::nullopt;
}

bool DeviceInfo::IsScreenLocked() const {
  absl::MutexLock lock(&mutex_);
  return session_manager_.IsScreenLocked();
}

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  absl::MutexLock lock(&mutex_);
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
  absl::MutexLock lock(&mutex_);
  session_manager_.UnregisterSessionListener(listener_name);
}

bool DeviceInfo::PreventSleep() {
  absl::MutexLock lock(&mutex_);
  return session_manager_.PreventSleep();
}

bool DeviceInfo::AllowSleep() {
  absl::MutexLock lock(&mutex_);
  return session_manager_.AllowSleep();
}

}  // namespace windows
}  // namespace nearby
