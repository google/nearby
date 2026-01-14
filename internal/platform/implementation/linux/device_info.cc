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

#include <pwd.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
void CurrentUserSession::RegisterScreenLockedListener(
    absl::string_view listener_name,
    std::function<void(api::DeviceInfo::ScreenStatus)> callback) {
  absl::MutexLock l(&screen_lock_listeners_mutex_);
  screen_lock_listeners_[listener_name] = std::move(callback);
}

void CurrentUserSession::UnregisterScreenLockedListener(
    absl::string_view listener_name) {
  absl::MutexLock l(&screen_lock_listeners_mutex_);
  screen_lock_listeners_.erase(listener_name);
}

void CurrentUserSession::onLock() {
  absl::ReaderMutexLock l(&screen_lock_listeners_mutex_);
  for (auto &[_, callback] : screen_lock_listeners_) {
    callback(api::DeviceInfo::ScreenStatus::kLocked);
  }
}

void CurrentUserSession::onUnlock() {
  absl::ReaderMutexLock l(&screen_lock_listeners_mutex_);
  for (auto &[_, callback] : screen_lock_listeners_) {
    callback(api::DeviceInfo::ScreenStatus::kUnlocked);
  }
}

DeviceInfo::DeviceInfo(std::shared_ptr<sdbus::IConnection> system_bus)
    : system_bus_(std::move(system_bus)),
      current_user_session_(std::make_unique<CurrentUserSession>(*system_bus_)),
      login_manager_(std::make_unique<LoginManager>(*system_bus_)) {}

std::optional<std::string> DeviceInfo::GetOsDeviceName() const {
  Hostnamed hostnamed(*system_bus_);
  try {
    return hostnamed.Hostname();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&hostnamed, "GetHostNameFqdn", e);
    return std::nullopt;
  }
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  Hostnamed hostnamed(*system_bus_);
  try {
    std::string chasis = hostnamed.Chassis();
    api::DeviceInfo::DeviceType device = api::DeviceInfo::DeviceType::kUnknown;
    if (chasis == "phone" || chasis == "handset") {
      device = api::DeviceInfo::DeviceType::kPhone;
    } else if (chasis == "laptop" || chasis == "desktop") {
      device = api::DeviceInfo::DeviceType::kLaptop;
    } else if (chasis == "tablet") {
      device = api::DeviceInfo::DeviceType::kTablet;
    }
    return device;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&hostnamed, "Chasis", e);
    return api::DeviceInfo::DeviceType::kUnknown;
  }
}


std::optional<FilePath> DeviceInfo::GetDownloadPath() const {
  char *dir = getenv("XDG_DOWNLOAD_DIR");
  return  FilePath(std::string(dir));
}

std::optional<FilePath> DeviceInfo::GetLocalAppDataPath() const {
  char *dir = getenv("XDG_CONFIG_HOME");
  if (dir == nullptr) {
    return FilePath("/tmp");
  }
  return FilePath(std::string((std::filesystem::path(std::string(dir)) / "Google Nearby")));
}

std::optional<FilePath> DeviceInfo::GetTemporaryPath() const {
  char *dir = getenv("XDG_RUNTIME_PATH");
  if (dir == nullptr) {
    return FilePath("/tmp");
  }
  return FilePath(std::string(std::filesystem::path(std::string(dir)) / "Google Nearby"));
}

std::optional<FilePath> DeviceInfo::GetLogPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == nullptr) {
    return FilePath("/tmp");
  }
  return FilePath(std::string(std::filesystem::path(std::string(dir)) / "Google Nearby" / "logs"));
}

std::optional<FilePath> DeviceInfo::GetCrashDumpPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == nullptr) {
    return FilePath("/tmp");
  }
  return FilePath(std::string(std::filesystem::path(std::string(dir)) / "Google Nearby" / "crashes"));
}

bool DeviceInfo::IsScreenLocked() const {
  try {
    return current_user_session_->LockedHint();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(current_user_session_, "LockedHint", e);
    return false;
  }
}

bool DeviceInfo::PreventSleep() {
  try {
    inhibit_fd_ = login_manager_->Inhibit("sleep", "Google Nearby",
                                          "Google Nearby", "block");
    return true;
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(login_manager_, "Inhibit", e);
    return false;
  }
}

bool DeviceInfo::AllowSleep() {
  if (!inhibit_fd_.has_value()) {
    LOG(ERROR) << __func__
                       << "No inhibit lock is acquired at the moment";
    return false;
  }

  inhibit_fd_.reset();
  return true;
}

}  // namespace linux
}  // namespace nearby
