#include <cstdlib>
#include <cstring>
#include <optional>
#include <pwd.h>
#include <string>
#include <sys/types.h>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <systemd/sd-login.h>

#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

const char *HOSTNAME_DEST = "org.freedesktop.hostname1";
const char *HOSTNAME_PATH = "/org/freedesktop/hostname1";
const char *HOSTNAME_INTERFACE = "org.freedesktop.hostname1";

const char *LOGIN_DEST = "org.freedesktop.login1";
const char *LOGIN_PATH = "/org/freedesktop/login1/session/_";
const char *LOGIN_INTERFACE = "org.freedesktop.login1.Session";

DeviceInfo::DeviceInfo(sdbus::IConnection &system_bus) {
  hostname_proxy_ =
      sdbus::createProxy(system_bus, HOSTNAME_DEST, HOSTNAME_PATH);
  hostname_proxy_->finishRegistration();
  login_proxy_ = sdbus::createProxy(system_bus, LOGIN_PATH, LOGIN_PATH);
  login_proxy_->finishRegistration();
}

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
  try {
    std::string hostname = hostname_proxy_->getProperty("PrettyHostname")
                               .onInterface(HOSTNAME_INTERFACE);
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(hostname);
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << "Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get PrettyHostname";
    return std::nullopt;
  }
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  try {
    std::string chasis = hostname_proxy_->getProperty("PrettyHostname")
                             .onInterface(HOSTNAME_INTERFACE);
    api::DeviceInfo::DeviceType device = api::DeviceInfo::DeviceType::kUnknown;
    if (chasis == "phone") {
      device = api::DeviceInfo::DeviceType::kPhone;
    } else if (chasis == "laptop" || chasis == "desktop") {
      device = api::DeviceInfo::DeviceType::kLaptop;
    } else if (chasis == "tablet") {
      device = api::DeviceInfo::DeviceType::kTablet;
    } else if (chasis == "handset") {
      device = api::DeviceInfo::DeviceType::kPhone;
    }
    return device;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << "Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get PrettyHostname";
    return api::DeviceInfo::DeviceType::kUnknown;
  }
}

std::optional<std::u16string> DeviceInfo::GetFullName() const {
  struct passwd *pwd = getpwuid(getuid());
  if (!pwd) {
    return std::nullopt;
  }
  char *name = strtok(pwd->pw_gecos, ",");

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.from_bytes(name ? name : pwd->pw_gecos);
}

std::optional<std::string> DeviceInfo::GetProfileUserName() const {
  struct passwd *pwd = getpwuid(getuid());
  if (!pwd) {
    return std::nullopt;
  }
  char *name = strtok(pwd->pw_gecos, ",");
  return std::string(name);
}

std::optional<std::filesystem::path> DeviceInfo::GetDownloadPath() const {
  char *dir = getenv("XDG_DOWNLOAD_DIR");
  if (dir == NULL) {
    std::filesystem::path home_path(std::string(getenv("HOME")));
    return home_path / "Desktop";
  }
  return std::filesystem::path(std::string(dir));
}

std::optional<std::filesystem::path> DeviceInfo::GetLocalAppDataPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == NULL) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "com.github.google.nearby";
}

std::optional<std::filesystem::path> DeviceInfo::GetTemporaryPath() const {
  char *dir = getenv("XDG_CACHE_HOME");
  if (dir == NULL) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "com.github.google.nearby";
}

std::optional<std::filesystem::path> DeviceInfo::GetLogPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == NULL) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "com.github.google.nearby" /
         "logs";
}

std::optional<std::filesystem::path> DeviceInfo::GetCrashDumpPath() const {
  char *dir = getenv("XDG_STATE_HOME");
  if (dir == NULL) {
    return std::filesystem::path("/tmp");
  }
  return std::filesystem::path(std::string(dir)) / "com.github.google.nearby" /
         "crashes";
}

bool DeviceInfo::IsScreenLocked() const {
  char *session = nullptr;
  if (sd_pid_get_session(getpid(), &session) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting session for current user";
    return false;
  }

  std::string session_path(LOGIN_PATH);
  session_path += session;
  free(session);

  try {
    bool locked =
        login_proxy_->getProperty("LockedHint").onInterface(LOGIN_INTERFACE);
    return locked;
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while trying to get LockedHint for session "
                       << session_path;
    return false;
  }
}
} // namespace linux
} // namespace nearby
