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

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/windows/session_manager.h"
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

template <typename T>
using IVectorView = winrt::Windows::Foundation::Collections::IVectorView<T>;

template <typename T>
using IAsyncOperation = winrt::Windows::Foundation::IAsyncOperation<T>;

constexpr char logs_relative_path[] = "Google\\Nearby\\Sharing\\Logs";
constexpr char crash_dumps_relative_path[] =
    "Google\\Nearby\\Sharing\\CrashDumps";

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
  DWORD size = 0;

  // Get length of the computer name.
  if (!GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size)) {
    if (GetLastError() != ERROR_MORE_DATA) {
      NEARBY_LOGS(ERROR) << ": Failed to get device name size, error:"
                         << GetLastError();
      return std::nullopt;
    }
  }

  WCHAR device_name[size];
  if (GetComputerNameExW(ComputerNameDnsHostname, device_name, &size)) {
    std::wstring wide_name(device_name);
    return std::u16string(wide_name.begin(), wide_name.end());
  }

  NEARBY_LOGS(ERROR) << ": Failed to get device name, error:" << GetLastError();
  return std::nullopt;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  // TODO(b/230132370): return correct device type on the Windows platform.
  return api::DeviceInfo::DeviceType::kLaptop;
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  return api::DeviceInfo::OsType::kWindows;
}

std::optional<std::u16string> DeviceInfo::GetFullName() const {
  // FindAllAsync finds all users that are using this app. When we "Switch User"
  // on Desktop,FindAllAsync() will still return the current user instead of all
  // of them because the users who are switched out are not using the apps of
  // the user who is switched in, so FindAllAsync() will not find them. (Under
  // the UWP application model, each process runs under its own user account.
  // That user account is different from the user account of the logged-in user.
  // Processes aren't owned by the logged-in user for purposes of isolation.)
  IVectorView<User> users =
      User::FindAllAsync(UserType::LocalUser,
                         UserAuthenticationStatus::LocallyAuthenticated)
          .get();
  if (users == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving locally authenticated user.";
    return std::nullopt;
  }

  // On Windows Desktop apps, the first Windows.System.User instance
  // returned in the IVectorView is always the current user.
  // https://github.com/microsoft/Windows-task-snippets/blob/master/tasks/User-info.md
  User current_user = users.GetAt(0);

  // Retrieve the human-readable properties for the current user
  IAsyncOperation<IInspectable> full_name_obj_async =
      current_user.GetPropertyAsync(KnownUserProperties::DisplayName());
  IInspectable full_name_obj = full_name_obj_async.get();
  if (full_name_obj == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error retrieving full name of user.";
    return std::nullopt;
  }
  winrt::hstring full_name = full_name_obj.as<winrt::hstring>();
  std::wstring wstr(full_name);
  std::u16string u16str(wstr.begin(), wstr.end());

  if (u16str.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error unboxing string value for full name of user.";
    return std::nullopt;
  }

  return u16str;
}

std::optional<std::u16string> DeviceInfo::GetGivenName() const {
  // FindAllAsync finds all users that are using this app. When we "Switch User"
  // on Desktop,FindAllAsync() will still return the current user instead of all
  // of them because the users who are switched out are not using the apps of
  // the user who is switched in, so FindAllAsync() will not find them. (Under
  // the UWP application model, each process runs under its own user account.
  // That user account is different from the user account of the logged-in user.
  // Processes aren't owned by the logged-in user for purposes of isolation.)
  IVectorView<User> users =
      User::FindAllAsync(UserType::LocalUser,
                         UserAuthenticationStatus::LocallyAuthenticated)
          .get();
  if (users == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving locally authenticated user.";
    return std::nullopt;
  }

  // On Windows Desktop apps, the first Windows.System.User instance
  // returned in the IVectorView is always the current user.
  // https://github.com/microsoft/Windows-task-snippets/blob/master/tasks/User-info.md
  User current_user = users.GetAt(0);

  // Retrieve the human-readable properties for the current user
  IAsyncOperation<IInspectable> given_name_obj_async =
      current_user.GetPropertyAsync(KnownUserProperties::FirstName());
  IInspectable given_name_obj = given_name_obj_async.get();
  if (given_name_obj == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error retrieving first name of user.";
    return std::nullopt;
  }
  winrt::hstring given_name = given_name_obj.as<winrt::hstring>();
  std::wstring wstr(given_name);
  std::u16string u16str(wstr.begin(), wstr.end());

  if (u16str.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error unboxing string value for first name of user.";
    return std::nullopt;
  }

  return u16str;
}

std::optional<std::u16string> DeviceInfo::GetLastName() const {
  // FindAllAsync finds all users that are using this app. When we "Switch User"
  // on Desktop,FindAllAsync() will still return the current user instead of all
  // of them because the users who are switched out are not using the apps of
  // the user who is switched in, so FindAllAsync() will not find them. (Under
  // the UWP application model, each process runs under its own user account.
  // That user account is different from the user account of the logged-in user.
  // Processes aren't owned by the logged-in user for purposes of isolation.)
  IVectorView<User> users =
      User::FindAllAsync(UserType::LocalUser,
                         UserAuthenticationStatus::LocallyAuthenticated)
          .get();
  if (users == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving locally authenticated user.";
    return std::nullopt;
  }

  // On Windows Desktop apps, the first Windows.System.User instance
  // returned in the IVectorView is always the current user.
  // https://github.com/microsoft/Windows-task-snippets/blob/master/tasks/User-info.md
  User current_user = users.GetAt(0);

  // Retrieve the human-readable properties for the current user
  IAsyncOperation<IInspectable> last_name_obj_async =
      current_user.GetPropertyAsync(KnownUserProperties::LastName());
  IInspectable last_name_obj = last_name_obj_async.get();
  if (last_name_obj == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error retrieving last name of user.";
    return std::nullopt;
  }
  winrt::hstring last_name = last_name_obj.as<winrt::hstring>();
  std::wstring wstr(last_name);
  std::u16string u16str(wstr.begin(), wstr.end());

  if (u16str.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error unboxing string value for last name of user.";
    return std::nullopt;
  }

  return u16str;
}

std::optional<std::string> DeviceInfo::GetProfileUserName() const {
  // FindAllAsync finds all users that are using this app. When we "Switch User"
  // on Desktop,FindAllAsync() will still return the current user instead of all
  // of them because the users who are switched out are not using the apps of
  // the user who is switched in, so FindAllAsync() will not find them. (Under
  // the UWP application model, each process runs under its own user account.
  // That user account is different from the user account of the logged-in user.
  // Processes aren't owned by the logged-in user for purposes of isolation.)
  IVectorView<User> users =
      User::FindAllAsync(UserType::LocalUser,
                         UserAuthenticationStatus::LocallyAuthenticated)
          .get();
  if (users == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving locally authenticated user.";
    return std::nullopt;
  }

  // On Windows Desktop apps, the first Windows.System.User instance
  // returned in the IVectorView is always the current user.
  // https://github.com/microsoft/Windows-task-snippets/blob/master/tasks/User-info.md
  User current_user = users.GetAt(0);

  // Retrieve the human-readable properties for the current user
  IAsyncOperation<IInspectable> account_name_obj_async =
      current_user.GetPropertyAsync(KnownUserProperties::AccountName());
  IInspectable account_name_obj = account_name_obj_async.get();
  if (account_name_obj == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error retrieving account name of user.";
    return std::nullopt;
  }
  winrt::hstring account_name = account_name_obj.as<winrt::hstring>();
  std::string account_name_string = winrt::to_string(account_name);

  if (account_name_string.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Error unboxing string value for profile username of user.";
    return std::nullopt;
  }

  return account_name_string;
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
  return std::filesystem::temp_directory_path();
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
