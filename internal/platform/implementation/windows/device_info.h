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

#ifndef PLATFORM_IMPL_WINDOWS_DEVICE_INFO_H_
#define PLATFORM_IMPL_WINDOWS_DEVICE_INFO_H_

#include <functional>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/windows/session_manager.h"

namespace nearby {
namespace windows {

class DeviceInfo : public api::DeviceInfo {
 public:
  ~DeviceInfo() override = default;

  std::optional<std::u16string> GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override;
  std::optional<std::u16string> GetFullName() const override;
  std::optional<std::u16string> GetGivenName() const override;
  std::optional<std::u16string> GetLastName() const override;
  std::optional<std::string> GetProfileUserName() const override;

  std::optional<std::filesystem::path> GetDownloadPath() const override;
  std::optional<std::filesystem::path> GetLocalAppDataPath() const override;
  std::optional<std::filesystem::path> GetCommonAppDataPath() const override;
  std::optional<std::filesystem::path> GetTemporaryPath() const override;
  std::optional<std::filesystem::path> GetLogPath() const override;
  std::optional<std::filesystem::path> GetCrashDumpPath() const override;

  bool IsScreenLocked() const override;
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override;
  void UnregisterScreenLockedListener(absl::string_view listener_name) override;

  bool PreventSleep() override;
  bool AllowSleep() override;

 private:
  mutable absl::Mutex mutex_;
  SessionManager session_manager_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_DEVICE_INFO_H_
