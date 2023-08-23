#ifndef PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
#define PLATFORM_IMPL_LINUX_DEVICE_INFO_H_

#include <optional>
#include <sdbus-c++/ProxyInterfaces.h>
#include <string>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/hostname_client_glue.h"
#include "internal/platform/implementation/linux/login_session_client_glue.h"

namespace nearby {
namespace linux {

class CurrentUserSession
    : public sdbus::ProxyInterfaces<org::freedesktop::login1::Session_proxy> {
public:
  CurrentUserSession(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.login1",
                        "/org/freedesktop/login1/session/auto") {
    registerProxy();
  }
  ~CurrentUserSession() { unregisterProxy(); }

  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback)
      ABSL_LOCKS_EXCLUDED(screen_lock_listeners_mutex_);
  void UnregisterScreenLockedListener(absl::string_view listener_name)
      ABSL_LOCKS_EXCLUDED(screen_lock_listeners_mutex_);

protected:
  void onPauseDevice(const uint32_t &major, const uint32_t &minor,
                     const std::string &type) override {}
  void onResumeDevice(const uint32_t &major, const uint32_t &minor,
                      const sdbus::UnixFd &fd) override {}

  void onLock() override ABSL_LOCKS_EXCLUDED(screen_lock_listeners_mutex_);
  void onUnlock() override ABSL_LOCKS_EXCLUDED(screen_lock_listeners_mutex_);

private:
  absl::Mutex screen_lock_listeners_mutex_;
  absl::flat_hash_map<std::string,
                      std::function<void(api::DeviceInfo::ScreenStatus)>>
      screen_lock_listeners_ ABSL_GUARDED_BY(screen_lock_listeners_mutex_);
};

class Hostnamed
    : public sdbus::ProxyInterfaces<org::freedesktop::hostname1_proxy> {
public:
  Hostnamed(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.hostname1",
                        "/org/freedesktop/hostname1") {
    registerProxy();
  }
  ~Hostnamed() { unregisterProxy(); }
};

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
  sdbus::IConnection &system_bus_;
  std::unique_ptr<CurrentUserSession> current_user_session_;
};
} // namespace linux
} // namespace nearby

#endif // PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
