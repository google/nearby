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
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
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

LRESULT CALLBACK NearbyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_WTSSESSION_CHANGE:
      if (wParam == WTS_SESSION_LOCK) {
        SessionManager::NotifySessionState(SessionManager::SessionState::kLock);
      } else if (wParam == WTS_SESSION_UNLOCK) {
        SessionManager::NotifySessionState(
            SessionManager::SessionState::kUnlock);
      }
      break;
  }

  // NOLINTNEXTLINE(misc-include-cleaner)
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

// Initialize class static variables.
ABSL_CONST_INIT absl::Mutex SessionManager::session_mutex_{absl::kConstInit};
HWND SessionManager::session_hwnd_ = nullptr;
SubmittableExecutor* SessionManager::session_thread_ = nullptr;
absl::flat_hash_map<std::string,
                    absl::AnyInvocable<void(SessionManager::SessionState)>>*
    SessionManager::session_callbacks_ = nullptr;

SessionManager::~SessionManager() { StopSession(); }

bool SessionManager::RegisterSessionListener(
    absl::string_view listener_name,
    absl::AnyInvocable<void(SessionState)> callback) {
  absl::MutexLock lock(&session_mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Registering listener: " << listener_name;

  // Create session thread if no running thread.
  if (session_thread_ == nullptr) {
    absl::Notification notification;
    session_thread_ = new SubmittableExecutor();
    session_callbacks_ = new absl::flat_hash_map<
        std::string, absl::AnyInvocable<void(SessionManager::SessionState)>>();
    session_thread_->Execute(
        [this, &notification]() { StartSession(notification); });
    notification.WaitForNotification();
    if (session_thread_ == nullptr) {
      return false;
    }
  }

  if (session_callbacks_->contains(listener_name) ||
      listeners_.contains(listener_name)) {
    return false;
  }

  session_callbacks_->emplace(listener_name, std::move(callback));
  listeners_.emplace(listener_name);
  NEARBY_LOGS(INFO) << __func__ << ": Session listener: " << listener_name
                    << " is registered.";
  return true;
}

bool SessionManager::UnregisterSessionListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(&session_mutex_);
  NEARBY_LOGS(INFO) << __func__
                    << ": Unregistering listener: " << listener_name;
  if (session_thread_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No running listener.";
    return false;
  }
  if (!session_callbacks_->contains(listener_name) ||
      !listeners_.contains(listener_name)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": No listener with name:" << listener_name;
    return false;
  }
  session_callbacks_->erase(listener_name);
  listeners_.erase(listener_name);

  if (!session_callbacks_->empty()) {
    NEARBY_LOGS(INFO) << __func__ << ": Session listener: " << listener_name
                      << " is unregistered.";
    return true;
  }

  CleanUp();
  NEARBY_LOGS(INFO) << __func__ << ": Session listener: " << listener_name
                    << " is unregistered.";
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

void SessionManager::NotifySessionState(SessionState state) {
  NEARBY_LOGS(INFO) << __func__
                    << ": Notifying session state: " << static_cast<int>(state);
  if (state == SessionManager::SessionState::kLock) {
    absl::MutexLock lock(&session_mutex_);
    for (auto& it : *SessionManager::session_callbacks_) {
      it.second(SessionManager::SessionState::kLock);
    }
  } else if (state == SessionManager::SessionState::kUnlock) {
    absl::MutexLock lock(&session_mutex_);
    for (auto& it : *SessionManager::session_callbacks_) {
      it.second(SessionManager::SessionState::kUnlock);
    }
  }
}

void SessionManager::StartSession(absl::Notification& notification) {
  session_hwnd_ = CreateNearbyWindow();
  if (session_hwnd_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to create session Window.";
    return;
  }

  if (!WTSRegisterSessionNotification(session_hwnd_, NOTIFY_FOR_THIS_SESSION)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ":Failed to register session notification.";
    return;
  }

  notification.Notify();

  NEARBY_LOGS(INFO) << __func__ << ": Session thread started.";

  // Main message loop
  MSG msg = {};
  // NOLINTNEXTLINE(misc-include-cleaner)
  while (GetMessage(&msg, session_hwnd_, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (!WTSUnRegisterSessionNotification(session_hwnd_)) {
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
  if (session_thread_ == nullptr) {
    return;
  }

  absl::MutexLock lock(&session_mutex_);
  if (session_thread_ == nullptr) {
    return;
  }
  for (const auto& it : listeners_) {
    session_callbacks_->erase(it);
  }

  listeners_.clear();
  if (!session_callbacks_->empty()) {
    return;
  }

  CleanUp();
}

void SessionManager::CleanUp() {
  if (session_hwnd_ != nullptr) {
    // Send message to destroy message window.
    // NOLINTNEXTLINE(misc-include-cleaner)
    PostMessageA(session_hwnd_, WM_DESTROY, 0, 0);
  }

  session_thread_->Shutdown();
  delete session_thread_;
  delete session_callbacks_;
  session_thread_ = nullptr;
  session_callbacks_ = nullptr;
  session_hwnd_ = nullptr;
}

}  // namespace windows
}  // namespace nearby
