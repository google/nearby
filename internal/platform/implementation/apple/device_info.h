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

#ifndef PLATFORM_IMPL_APPLE_DEVICE_INFO_H_
#define PLATFORM_IMPL_APPLE_DEVICE_INFO_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace apple {

class DeviceInfo : public api::DeviceInfo {
 public:
  std::optional<std::string> GetOsDeviceName() const override;

  api::DeviceInfo::DeviceType GetDeviceType() const override;

  api::DeviceInfo::OsType GetOsType() const override;

  std::optional<FilePath> GetDownloadPath() const override;

  std::optional<FilePath> GetLocalAppDataPath() const override;

  std::optional<FilePath> GetCommonAppDataPath() const override;

  std::optional<FilePath> GetTemporaryPath() const override;

  std::optional<FilePath> GetLogPath() const override;

  std::optional<FilePath> GetCrashDumpPath() const override;

  bool IsScreenLocked() const override;

  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override;

  void UnregisterScreenLockedListener(absl::string_view listener_name) override;

  // Request the system to prevent the device from going to sleep.
  //
  // To allow the system to sleep again, call @c AllowSleep().
  //
  // Returns @c true on success or if the platform does not allow preventing
  // sleep. Returns @c false on any other error.
  bool PreventSleep() override;

  // Release any requests preventing the device from going to sleep.
  //
  // Returns @c true on success or if the system is already allowed to sleep.
  // Returns @c false if an error occurred.
  bool AllowSleep() override;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_DEVICE_INFO_H_
