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

#ifndef PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
#define PLATFORM_IMPL_LINUX_DEVICE_INFO_H_

#include <optional>
#include <string>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/generated/dbus/hostname/hostname_client.h"
#include "internal/platform/implementation/linux/generated/dbus/login/login_manager_client.h"
#include "internal/platform/implementation/linux/generated/dbus/login/login_session_client.h"

namespace nearby {
namespace linux {

class CurrentUserSession final
    : public sdbus::ProxyInterfaces<org::freedesktop::login1::Session_proxy> {
 public:
  CurrentUserSession(const CurrentUserSession &) = delete;
  CurrentUserSession(CurrentUserSession &&) = delete;
  CurrentUserSession &operator=(const CurrentUserSession &) = delete;
  CurrentUserSession &operator=(CurrentUserSession &&) = delete;
  ~CurrentUserSession() { unregisterProxy(); }
  explicit CurrentUserSession(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.login1",
                        "/org/freedesktop/login1/session/auto") {
    registerProxy();
  }

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
  Hostnamed(const Hostnamed &) = delete;
  Hostnamed(Hostnamed &&) = delete;
  Hostnamed &operator=(const Hostnamed &) = delete;
  Hostnamed &operator=(Hostnamed &&) = delete;
  explicit Hostnamed(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.hostname1",
                        "/org/freedesktop/hostname1") {
    registerProxy();
  }
  ~Hostnamed() { unregisterProxy(); }
};

class LoginManager final
    : public sdbus::ProxyInterfaces<org::freedesktop::login1::Manager_proxy> {
 public:
  LoginManager(const LoginManager &) = delete;
  LoginManager(LoginManager &&) = delete;
  LoginManager &operator=(const LoginManager &) = delete;
  LoginManager &operator=(LoginManager &&) = delete;
  explicit LoginManager(sdbus::IConnection &system_bus)
      : ProxyInterfaces(system_bus, "org.freedesktop.login1",
                        "/org/freedesktop/login1") {
    registerProxy();
  }
  ~LoginManager() { unregisterProxy(); }

 protected:
  void onSessionNew(const std::string &session_id,
                    const sdbus::ObjectPath &object_path) override {}
  void onSessionRemoved(const std::string &session_id,
                        const sdbus::ObjectPath &object_path) override {}
  void onUserNew(const uint32_t &uid,
                 const sdbus::ObjectPath &object_path) override {}
  void onUserRemoved(const uint32_t &uid,
                     const sdbus::ObjectPath &object_path) override {}
  void onSeatNew(const std::string &seat_id,
                 const sdbus::ObjectPath &object_path) override {}
  void onSeatRemoved(const std::string &seat_id,
                     const sdbus::ObjectPath &object_path) override {}
  void onPrepareForShutdown(const bool &start) override {}
  void onPrepareForSleep(const bool &start) override {}
};

class DeviceInfo final : public api::DeviceInfo {
 public:
  explicit DeviceInfo(std::shared_ptr<sdbus::IConnection> system_bus);

  std::optional<std::string> GetOsDeviceName() const override;
  api::DeviceInfo::DeviceType GetDeviceType() const override;
  api::DeviceInfo::OsType GetOsType() const override {
    return api::DeviceInfo::OsType::kWindows;  // Or ChromeOS?
  }

  std::optional<FilePath> GetDownloadPath() const override;
  std::optional<FilePath> GetLocalAppDataPath() const override;
  std::optional<FilePath> GetCommonAppDataPath() const override {
    return std::nullopt;
  };
  std::optional<FilePath> GetTemporaryPath() const override;
  std::optional<FilePath> GetLogPath() const override;
  std::optional<FilePath> GetCrashDumpPath() const override;

  bool IsScreenLocked() const override;
  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override {
    current_user_session_->RegisterScreenLockedListener(listener_name,
                                                        std::move(callback));
  }
  void UnregisterScreenLockedListener(
      absl::string_view listener_name) override {
    current_user_session_->UnregisterScreenLockedListener(listener_name);
  }

  bool PreventSleep() override;
  bool AllowSleep() override;

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::unique_ptr<CurrentUserSession> current_user_session_;
  std::unique_ptr<LoginManager> login_manager_;
  std::optional<sdbus::UnixFd> inhibit_fd_;
};
}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_DEVICE_INFO_H_
