// Copyright 2023 Google LLC
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

#include "internal/platform/implementation/windows/session_manager.h"

#include <Windows.h>
#include <wtsapi32.h>

#include <string>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

constexpr char kMessageWindowClass[] = "Nearby Message Window Class";
constexpr char kMessageWindowTitle[] = "Nearby Message Dummy Window";

// Define global static variables.
ABSL_CONST_INIT absl::Mutex kSessionMutex(absl::kConstInit);
HWND kSessionHwnd = nullptr;
SubmittableExecutor* kSessionThread = nullptr;
absl::flat_hash_map<std::string,
                    absl::AnyInvocable<void(SessionManager::SessionState)>>*
    kSessionCallbacks = nullptr;

LRESULT CALLBACK NearbyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_WTSSESSION_CHANGE:
      if (wParam == WTS_SESSION_LOCK) {
        absl::MutexLock lock(&kSessionMutex);
        for (auto& it : *kSessionCallbacks) {
          it.second(SessionManager::SessionState::kLock);
        }
      } else if (wParam == WTS_SESSION_UNLOCK) {
        absl::MutexLock lock(&kSessionMutex);
        for (auto& it : *kSessionCallbacks) {
          it.second(SessionManager::SessionState::kUnlock);
        }
      }
      return 0;
    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

HWND CreateNearbyWindow() {
  HINSTANCE instance = (HINSTANCE)GetModuleHandle(nullptr);

  WNDCLASS window_class = {};
  window_class.lpfnWndProc = NearbyWindowProc;
  window_class.hInstance = instance;
  window_class.lpszClassName = kMessageWindowClass;

  RegisterClass(&window_class);

  HWND hwnd =
      CreateWindowA(kMessageWindowClass, kMessageWindowTitle, /*dwStyle=*/0,
                    /*X=*/0, /*Y=*/0, /*nWidth=*/0, /*nHeight=*/0, HWND_MESSAGE,
                    /*hMenu=*/nullptr, instance,
                    /*lpParam=*/nullptr);

  return hwnd;
}

}  // namespace

SessionManager::~SessionManager() { StopSession(); }

bool SessionManager::RegisterSessionListener(
    absl::string_view listener_name,
    absl::AnyInvocable<void(SessionState)> callback) {
  absl::MutexLock lock(&kSessionMutex);

  // Create session thread if no running thread.
  if (kSessionThread == nullptr) {
    absl::Notification notification;
    kSessionThread = new SubmittableExecutor();
    kSessionCallbacks = new absl::flat_hash_map<
        std::string, absl::AnyInvocable<void(SessionManager::SessionState)>>();
    kSessionThread->Execute(
        [this, &notification]() { StartSession(notification); });
    notification.WaitForNotification();
    if (kSessionThread == nullptr) {
      return false;
    }
  }

  if (kSessionCallbacks->contains(listener_name) ||
      listeners_.contains(listener_name)) {
    return false;
  }

  kSessionCallbacks->emplace(listener_name, std::move(callback));
  listeners_.emplace(listener_name);
  return true;
}

bool SessionManager::UnregisterSessionListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(&kSessionMutex);
  if (kSessionThread == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No running listener.";
    return false;
  }
  if (!kSessionCallbacks->contains(listener_name) ||
      !listeners_.contains(listener_name)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": No listener with name:" << listener_name;
    return false;
  }
  kSessionCallbacks->erase(listener_name);
  listeners_.erase(listener_name);

  if (!kSessionCallbacks->empty()) {
    return true;
  }

  CleanUp();
  return true;
}

bool SessionManager::IsScreenLocked() const {
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

bool SessionManager::PreventSleep() const {
  EXECUTION_STATE execution_state =
      SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
  if (execution_state == 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set execution state of the thread.";
    return false;
  }
  return true;
}

bool SessionManager::AllowSleep() const {
  EXECUTION_STATE execution_state = SetThreadExecutionState(ES_CONTINUOUS);
  if (execution_state == 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to set execution state of the thread.";
    return false;
  }
  return true;
}

void SessionManager::StartSession(absl::Notification& notification) {
  kSessionHwnd = CreateNearbyWindow();
  if (kSessionHwnd == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to create session Window.";
    return;
  }

  if (!WTSRegisterSessionNotification(kSessionHwnd, NOTIFY_FOR_THIS_SESSION)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ":Failed to register session notification.";
    return;
  }

  notification.Notify();

  // Main message loop
  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (!WTSUnRegisterSessionNotification(kSessionHwnd)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to register session notification.";
    return;
  }

  if (!UnregisterClassA(/*lpClassName=*/kMessageWindowClass,
                        /*hInstance=*/(HINSTANCE)GetModuleHandle(nullptr))) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to unregister window class.";
  }

  NEARBY_LOGS(INFO) << __func__ << ": Completed Message loop.";
}

void SessionManager::StopSession() {
  absl::MutexLock lock(&kSessionMutex);
  if (kSessionThread == nullptr) {
    return;
  }
  for (const auto& it : listeners_) {
    kSessionCallbacks->erase(it);
  }

  listeners_.clear();
  if (!kSessionCallbacks->empty()) {
    return;
  }

  CleanUp();
}

void SessionManager::CleanUp() {
  if (kSessionHwnd != nullptr) {
    // Send message to destroy message window.
    PostMessageA(kSessionHwnd, WM_DESTROY, 0, 0);
  }

  kSessionThread->Shutdown();
  delete kSessionThread;
  delete kSessionCallbacks;
  kSessionThread = nullptr;
  kSessionCallbacks = nullptr;
  kSessionHwnd = nullptr;
}

}  // namespace windows
}  // namespace nearby
