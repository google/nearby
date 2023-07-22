#include <cstring>
#include <optional>
#include <string>

#include <pwd.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>

#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {

namespace linux {

const char *HOSTNAME_DEST = "org.freedesktop.hostname1";
const char *HOSTNAME_PATH = "/org/freedesktop/hostname1";
const char *HOSTNAME_INTERFACE = "org.freedesktop.hostname1";

std::optional<std::u16string> DeviceInfo::GetOsDeviceName() const {
  sd_bus *bus = nullptr;
  if (sd_bus_default_system(&bus) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error connecting to systemd bus.";
    return std::nullopt;
  }

  sd_bus_error err = SD_BUS_ERROR_NULL;

  char *hostname = nullptr;
  if (sd_bus_get_property_string(bus, HOSTNAME_DEST, HOSTNAME_PATH,
                                 HOSTNAME_INTERFACE, "PrettyHostname", &err,
                                 &hostname) < 0) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Error getting PrettyHostname from org.freedesktop.hostname1: "
        << err.message;
  }
  if (!hostname || hostname[0] == '\0') {
    int ret = sd_bus_get_property_string(bus, HOSTNAME_DEST, HOSTNAME_PATH,
                                         HOSTNAME_INTERFACE, "Hostname", &err,
                                         &hostname);
    if (ret < 0) {
      NEARBY_LOGS(ERROR)
          << __func__
          << ": Error getting Hostname from org.freedesktop.hostname1: "
          << err.message;
      sd_bus_error_free(&err);
      sd_bus_unref(bus);
      return std::nullopt;
    }
  }

  sd_bus_error_free(&err);
  sd_bus_unref(bus);

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;

  std::u16string name = convert.from_bytes(hostname);
  free(hostname);
  return name;
}

api::DeviceInfo::DeviceType DeviceInfo::GetDeviceType() const {
  sd_bus *bus;
  if (sd_bus_default_system(&bus) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error connecting to systemd bus.";
    return api::DeviceInfo::DeviceType::kUnknown;
  }
  sd_bus_error err = SD_BUS_ERROR_NULL;
  char *chasis = nullptr;

  if (sd_bus_get_property_string(bus, HOSTNAME_DEST, HOSTNAME_PATH,
                                 HOSTNAME_INTERFACE, "Chasis", &err,
                                 &chasis) < 0) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": Error getting Chasis from org.freedesktop.hostname1: "
        << err.message;
    sd_bus_error_free(&err);
    sd_bus_unref(bus);
    return api::DeviceInfo::DeviceType::kUnknown;
  }

  sd_bus_unref(bus);

  api::DeviceInfo::DeviceType device = api::DeviceInfo::DeviceType::kUnknown;

  if (strcmp(chasis, "phone") == 0) {
    device = api::DeviceInfo::DeviceType::kPhone;
  } else if (strcmp(chasis, "laptop") == 0 || strcmp(chasis, "desktop") == 0) {
    device = api::DeviceInfo::DeviceType::kLaptop;
  } else if (strcmp(chasis, "tablet") == 0) {
    device = api::DeviceInfo::DeviceType::kTablet;
  } else if (strcmp(chasis, "handset") == 0) {
    device = api::DeviceInfo::DeviceType::kPhone;
  }
  free(chasis);
  return device;
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
} // namespace linux
} // namespace nearby
