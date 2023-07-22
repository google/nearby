#ifndef PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
#define PLATFORM_IMPL_LINUX_DEVICE_INFO_H_

#include <systemd/sd-bus.h>

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace linux {

class DeviceInfo : public api::DeviceInfo {
public:
  ~DeviceInfo() override;

  std::optional<std::u16string> GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override {
    return api::DeviceInfo::OsType::kWindows; // Or ChromeOS?
  }
  std::optional<std::u16string> GetFullName() const override;
  std::optional<std::u16string> GetGivenName() const override {
    return GetFullName();
  }
  std::optional<std::u16string> GetLastName() const override {
    return GetFullName();
  }
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

  sd_bus *system_bus;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
