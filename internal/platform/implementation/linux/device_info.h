#ifndef PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
#define PLATFORM_IMPL_LINUX_DEVICE_INFO_H_

#include <optional>
#include <string>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace linux {

class DeviceInfo : public api::DeviceInfo {
public:
  DeviceInfo(sdbus::IConnection &system_bus);
  ~DeviceInfo() override = default;

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
  std::optional<std::filesystem::path> GetCommonAppDataPath() const override {
    return std::nullopt;
  };
  std::optional<std::filesystem::path> GetTemporaryPath() const override;
  std::optional<std::filesystem::path> GetLogPath() const override;
  std::optional<std::filesystem::path> GetCrashDumpPath() const override;

  bool IsScreenLocked() const override;
  // TODO: Implement listening to logind for changes to LockedState.
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override{};
  void
  UnregisterScreenLockedListener(absl::string_view listener_name) override{};

private:
  std::unique_ptr<sdbus::IProxy> hostname_proxy_;
  std::unique_ptr<sdbus::IProxy> login_proxy_;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
