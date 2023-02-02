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

#include "internal/platform/implementation/windows/device_info.h"

#include <shlobj_core.h>
#include <windows.h>
#include <wtsapi32.h>

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/implementation/device_info.h"
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

constexpr char window_class_name[] = "NearbySharingDLL_MessageWindowClass";
constexpr char window_name[] = "NearbySharingDLL_MessageWindow";
constexpr char kLogsRelativePath[] = "Google\\Nearby\\Sharing\\Logs";
constexpr char kCrashDumpsRelativePath[] =
    "Google\\Nearby\\Sharing\\CrashDumps";

namespace {
// This WindowProc method must be static for the successful initialization of
// WNDCLASS
//    window_class.lpfnWndProc = (WNDPROC) &DeviceInfo::WindowProc;
// where a WNDPROC typed function pointer is expected
//    typedef LRESULT (CALLBACK* WNDPROC)(HWND,UINT,WPARAM,LPARAM)
// the calling convention used here CALLBACK is a macro defined as
//    #define CALLBACK __stdcall
//
// If WindProc is not static and defined as a member function, it uses the
// __thiscall calling convention instead
// https://docs.microsoft.com/en-us/cpp/cpp/thiscall?view=msvc-170
// https://isocpp.org/wiki/faq/pointers-to-members
// https://en.cppreference.com/w/cpp/language/pointer
//
// This is problematic because the function pointer now looks like this
//    typedef LRESULT (CALLBACK* DeviceInfo_WNDPROC)(DeviceInfo*
//    this,HWND,UINT,WPARAM,LPARAM)
// which causes casting errors
LRESULT CALLBACK WindowProc(HWND window_handle, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  DeviceInfo* self = reinterpret_cast<DeviceInfo*>(
      GetWindowLongPtr(window_handle, GWLP_USERDATA));
  CREATESTRUCT* create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
  LONG_PTR result = 0L;
  switch (message) {
    case WM_CREATE:
      self = reinterpret_cast<DeviceInfo*>(create_struct->lpCreateParams);
      self->message_window_handle_ = window_handle;

      // Store pointer to the self to the window's user data.
      SetLastError(0);
      result = SetWindowLongPtr(window_handle, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(self));
      if (result == 0 && GetLastError() != 0) {
        NEARBY_LOGS(ERROR)
            << __func__
            << ": Error connecting message window to Nearby Sharing DLL.";
      }
      break;
    case WM_WTSSESSION_CHANGE:
      if (self) {
        switch (wparam) {
          case WTS_SESSION_LOCK:
            for (auto& listener : self->screen_locked_listeners_) {
              listener.second(api::DeviceInfo::ScreenStatus::
                                  kLocked);  // Trigger registered callbacks
            }
            break;
          case WTS_SESSION_UNLOCK:
            for (auto& listener : self->screen_locked_listeners_) {
              listener.second(api::DeviceInfo::ScreenStatus::
                                  kUnlocked);  // Trigger registered callbacks
            }
            break;
        }
      }
      break;
    case WM_DESTROY:
      SetLastError(0);
      result = SetWindowLongPtr(window_handle, GWLP_USERDATA, NULL);
      if (result == 0 && GetLastError() != 0) {
        NEARBY_LOGS(ERROR)
            << __func__
            << ": Error disconnecting message window to Nearby Sharing DLL.";
      }
      break;
  }

  return DefWindowProc(window_handle, message, wparam, lparam);
}
}  // namespace

DeviceInfo::~DeviceInfo() {
  UnregisterClass(MAKEINTATOM(registered_class_), instance_);
}

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
  std::string path;
  path.resize(MAX_PATH);
  HRESULT result = SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                    SHGFP_TYPE_CURRENT, path.data());
  if (result == S_OK) {
    path.resize(strlen(path.data()));
    return std::filesystem::path(path);
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCommonAppDataPath() const {
  PWSTR path;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    std::wstring download_path{path};
    CoTaskMemFree(path);
    return std::filesystem::path(download_path);
  }

  CoTaskMemFree(path);
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  return std::filesystem::temp_directory_path();
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  PWSTR path;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    std::filesystem::path prefixPath = path;
    CoTaskMemFree(path);
    return std::filesystem::path(prefixPath / kLogsRelativePath);
  }
  CoTaskMemFree(path);
  return std::nullopt;
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  PWSTR path;
  HRESULT result = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                        /*hToken=*/nullptr, &path);
  if (result == S_OK) {
    std::filesystem::path prefixPath = path;
    CoTaskMemFree(path);
    return std::filesystem::path(prefixPath / kCrashDumpsRelativePath);
  }
  CoTaskMemFree(path);
  return std::nullopt;
}

bool DeviceInfo::IsScreenLocked() const {
  DWORD session_id = WTSGetActiveConsoleSessionId();
  WTS_INFO_CLASS wts_info_class = WTSSessionInfoEx;
  LPTSTR session_info_buffer = nullptr;
  DWORD session_info_buffer_size_bytes = 0;

  WTSINFOEXW* wts_info = nullptr;
  LONG session_state = WTS_SESSIONSTATE_UNKNOWN;

  if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, session_id,
                                 wts_info_class, &session_info_buffer,
                                 &session_info_buffer_size_bytes)) {
    if (session_info_buffer_size_bytes > 0) {
      wts_info = (WTSINFOEXW*)session_info_buffer;
      if (wts_info->Level == 1) {
        session_state = wts_info->Data.WTSInfoExLevel1.SessionFlags;
      }
    }
    WTSFreeMemory(session_info_buffer);
    session_info_buffer = nullptr;
  }

  return (session_state == WTS_SESSIONSTATE_LOCK);
}

void DeviceInfo::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  if (message_window_handle_ == nullptr) {
    instance_ = (HINSTANCE)GetModuleHandle(NULL);

    WNDCLASS window_class;
    window_class.style = 0;
    window_class.lpfnWndProc = (WNDPROC)&WindowProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance_;
    window_class.hIcon = nullptr;
    window_class.hCursor = nullptr;
    window_class.hbrBackground = nullptr;
    window_class.lpszMenuName = nullptr;
    window_class.lpszClassName = window_class_name;

    registered_class_ = RegisterClass(&window_class);

    message_window_handle_ = CreateWindow(
        MAKEINTATOM(registered_class_),  // class atom from RegisterClass
        window_name,                     // window name
        0,                               // window style
        0,                               // initial x position of window
        0,                               // initial y position of window
        0,                               // width
        0,                               // height
        HWND_MESSAGE,                    // handle to the parent of window
                                         // (message-only window in this case)
        nullptr,                         // handle to a menu
        instance_,                       // handle to the instance of the module
                                         // associated to the window
        this);  // pointer to be passed to the window for additional data

    if (!message_window_handle_) {
      NEARBY_LOGS(ERROR)
          << __func__
          << ": Failed to create message window for Nearby Sharing DLL.";
    }
  }

  screen_locked_listeners_.emplace(listener_name, callback);
}

void DeviceInfo::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  screen_locked_listeners_.erase(listener_name);
}

}  // namespace windows
}  // namespace nearby
