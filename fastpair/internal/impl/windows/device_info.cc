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

#include "fastpair/internal/impl/windows/device_info.h"

#include <shlobj_core.h>
#include <windows.h>
#include <wtsapi32.h>

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/internal/api/device_info.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.System.h"

namespace nearby {
namespace windows {

constexpr char window_class_name[] = "FastPairDLL_MessageWindowClass";
constexpr char window_name[] = "FastPairDLL_MessageWindow";

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
            << ": Error connecting message window to Fast Pair DLL.";
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
            << ": Error disconnecting message window to Fast Pair DLL.";
      }
      break;
  }

  return DefWindowProc(window_handle, message, wparam, lparam);
}
}  // namespace

DeviceInfo::~DeviceInfo() {
  UnregisterClass(MAKEINTATOM(registered_class_), instance_);
}

api::DeviceInfo::OsType DeviceInfo::GetOsType() const {
  return api::DeviceInfo::OsType::kWindows;
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
  bool isScreenLocked = session_state == WTS_SESSIONSTATE_LOCK;

  NEARBY_LOGS(INFO) << __func__ << "ScreenStatus: "
                    << (isScreenLocked ? "Locked" : "NotLocked");

  return isScreenLocked;
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
          << __func__ << ": Failed to create message window for Fast Pair DLL.";
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
