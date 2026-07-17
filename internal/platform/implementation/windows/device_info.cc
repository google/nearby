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

// clang-format off
#include <shlobj_core.h>
#include <windows.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <powerbase.h>
// clang-format on

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/files.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/windows/device_paths.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {
using ::nearby::windows::string_utils::WideStringToString;
}  // namespace

DeviceInfo::~DeviceInfo() {
  if (suspend_resume_notification_handle_ != nullptr) {
    PowerUnregisterSuspendResumeNotification(
        suspend_resume_notification_handle_);
  }
}

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

ULONG DeviceInfo::PowerSuspendResumeCallback(PVOID context, ULONG type,
                                             PVOID setting) {
  api::DeviceInfo::SuspendResumeEvent event;
  switch (type) {
    case PBT_APMSUSPEND:
      event = api::DeviceInfo::SuspendResumeEvent::kSuspend;
      break;
    case PBT_APMRESUMESUSPEND:
      event = api::DeviceInfo::SuspendResumeEvent::kResume;
      break;
    default:
      return 0;
  }
  DeviceInfo* device_info = static_cast<DeviceInfo*>(context);
  device_info->OnSuspendResumeEvent(event);
  return 0;
}

int64_t DeviceInfo::RegisterSuspendResumeListener(
    std::function<void(api::DeviceInfo::SuspendResumeEvent)> callback) {
  absl::MutexLock lock(suspend_resume_mutex_);
  int64_t listener_id = ++next_suspend_resume_listener_id_;
  suspend_resume_listeners_.emplace(listener_id, std::move(callback));
  if (suspend_resume_listeners_.size() == 1) {
    DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS subscribe_params;
    subscribe_params.Callback = PowerSuspendResumeCallback;
    subscribe_params.Context = this;
    PowerRegisterSuspendResumeNotification(
        DEVICE_NOTIFY_CALLBACK, &subscribe_params,
        &suspend_resume_notification_handle_);
  }
  return listener_id;
}

void DeviceInfo::UnregisterSuspendResumeListener(int64_t listener_id) {
  absl::MutexLock lock(suspend_resume_mutex_);
  suspend_resume_listeners_.erase(listener_id);
  if (suspend_resume_listeners_.empty()) {
    if (suspend_resume_notification_handle_ != nullptr) {
    PowerUnregisterSuspendResumeNotification(
        suspend_resume_notification_handle_);
    }
    suspend_resume_notification_handle_ = nullptr;
  }
}

void DeviceInfo::OnSuspendResumeEvent(
    api::DeviceInfo::SuspendResumeEvent event) {
  LOG(INFO) << "OnSuspendResumeEvent: "
            << (event == DeviceInfo::SuspendResumeEvent::kSuspend ? "kSuspend"
                                                                  : "kResume");
  absl::MutexLock lock(suspend_resume_mutex_);
  for (auto& it : suspend_resume_listeners_) {
    it.second(event);
  }
}

}  // namespace nearby::windows
