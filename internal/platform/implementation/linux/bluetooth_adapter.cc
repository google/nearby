#include <systemd/sd-bus.h>

#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

using namespace api;

bool BluetoothAdapter::SetStatus(Status status) {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;

  if (sd_bus_set_property(
          system_bus, BLUEZ_SERVICE, "/org/bluez/hci0", BLUEZ_ADAPTER_INTERFACE,
          "Powered", &err, "b",
          status == api::BluetoothAdapter::Status::kEnabled ? 1 : 0) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error setting adaptor status: " << err.message;
    return false;
  }
  return true;
}

bool BluetoothAdapter::IsEnabled() const {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  int enabled = 0;

  if (sd_bus_get_property_trivial(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                                  BLUEZ_ADAPTER_INTERFACE, "Powered", &err, 'b',
                                  &enabled) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting adaptor status: " << err.message;
  }
  return enabled;
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  int powered = 0;
  int discoverable = 0;

  if (sd_bus_get_property_trivial(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                                  BLUEZ_ADAPTER_INTERFACE, "Powered", &err, 'b',
                                  &powered) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting adaptor status: " << err.message;
    return ScanMode::kUnknown;
  }
  if (!powered) {
    return ScanMode::kNone;
  }

  if (sd_bus_get_property_trivial(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                                  BLUEZ_ADAPTER_INTERFACE, "Discoverable", &err,
                                  'b', &powered) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting adaptor's discoverable status: "
                       << err.message;
    return ScanMode::kUnknown;
  }
  return discoverable ? ScanMode::kConnectableDiscoverable
                      : ScanMode::kConnectable;
}

bool BluetoothAdapter::SetScanMode(ScanMode scan_mode) {
  switch (scan_mode) {
  case ScanMode::kConnectable:
    return SetStatus(Status::kEnabled);
  case ScanMode::kConnectableDiscoverable: {
    if (!SetStatus(Status::kEnabled)) {
      return false;
    }
    __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
        SD_BUS_ERROR_NULL;
    if (sd_bus_set_property(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                            BLUEZ_ADAPTER_INTERFACE, "Discoverable", &err, "b",
                            1) < 0) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Error setting adapter's discoverable status: "
                         << err.message;
      return false;
    }
    return true;
  }
  case ScanMode::kNone:
    return SetStatus(Status::kDisabled);
  default:
    return false;
  }
}

std::string BluetoothAdapter::GetName() const {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  char *cname = nullptr;
  if (sd_bus_get_property_string(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                                 BLUEZ_ADAPTER_INTERFACE, "Alias", &err,
                                 &cname) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting adapter's name: " << err.message;
    return std::string();
  }
  std::string name(cname);
  free(cname);
  return name;
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  if (persist) {
    __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
        SD_BUS_ERROR_NULL;
    std::string pretty_hostname(name);
    if (sd_bus_set_property(system_bus, "org.freedesktop.hostname1",
                            "/org/freedesktop/hostname1",
                            "org.freedesktop.hostname1", "PrettyHostname", &err,
                            "s", pretty_hostname.c_str()) < 0) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Error setting PrettyHostname: " << err.message;
    }
  }
  return SetName(name);
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  std::string alias(name);
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  if (sd_bus_set_property(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                          BLUEZ_ADAPTER_INTERFACE, "Alias", &err, "s",
                          alias.c_str()) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error setting adapter's name: " << err.message;
    return false;
  }
  return true;
}

std::string BluetoothAdapter::GetMacAddress() const {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  char *caddr = nullptr;
  if (sd_bus_get_property_string(system_bus, BLUEZ_SERVICE, "/org/bluez/hci0",
                                 BLUEZ_ADAPTER_INTERFACE, "Address", &err,
                                 &caddr) < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error getting adapter's name: " << err.message;
    return std::string();
  }
  std::string addr(caddr);
  free(caddr);
  return addr;
}

} // namespace linux
} // namespace nearby
