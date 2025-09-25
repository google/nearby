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

#ifndef PLATFORM_IMPL_LINUX_INFO_H_
#define PLATFORM_IMPL_LINUX_INFO_H_

#include <functional>
#include <optional>
#include <string>
#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace linux {

class DeviceInfo: public api::DeviceInfo {
 public:
  ~DeviceInfo() = default;

  // Gets device name.
  std::optional<std::string> GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override
  {
    return api::DeviceInfo::OsType::kWindows; //TODO: should probably change to linux
  };

  // Gets known paths of current user.
  std::optional<FilePath> GetDownloadPath() const override;
  std::optional<FilePath> GetLocalAppDataPath() const override;
  std::optional<FilePath> GetCommonAppDataPath() const override;
  std::optional<FilePath> GetTemporaryPath() const override;
  std::optional<FilePath> GetLogPath() const override;
  std::optional<FilePath> GetCrashDumpPath() const override;

  // Monitor screen status
  bool IsScreenLocked() const override
  {
    return false;
  };
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override {return;};
  void UnregisterScreenLockedListener(
      absl::string_view listener_name) override { return;};

  // Control device sleep
  bool PreventSleep() override {return true;};
  bool AllowSleep() override { return true;};
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_INFO_H_
