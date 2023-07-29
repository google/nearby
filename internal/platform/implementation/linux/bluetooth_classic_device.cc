#include <systemd/sd-bus.h>

#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

BluetoothDevice::BluetoothDevice(absl::string_view adapter,
                                 absl::string_view address) {
  mac_addr_ = std::string(address);
  object_path_ = absl::Substitute("/org/bluez/$0/dev_$1", adapter,
                                  absl::StrReplaceAll(address, {{":", "_"}}));
  if (sd_bus_default_system(&system_bus) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error connecting to system bus";
  }
}

BluetoothDevice::BluetoothDevice(absl::string_view device_object_path) {
  if (sd_bus_default_system(&system_bus) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error connecting to system bus";
    return;
  }

  object_path_ = device_object_path;  
}

std::string BluetoothDevice::GetName() const {
  if (!system_bus) {
    return std::string();
  }
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  char *cname = nullptr;
  if (sd_bus_get_property_string(system_bus, BLUEZ_SERVICE,
                                 object_path_.c_str(), BLUEZ_DEVICE_INTERFACE,
                                 "Alias", &err, &cname) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error getting alias for device "
                       << object_path_ << " :" << err.message;
    return std::string();
  }

  std::string name(cname);
  free(cname);
  return name;
}

std::string BluetoothDevice::GetMacAddress() const {
  if (!system_bus) {
    return std::string();
  }

  if (!mac_addr_.empty()) {
    return mac_addr_;
  }
  
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  char *c_addr = nullptr;
  if (sd_bus_get_property_string(system_bus, BLUEZ_SERVICE,
                                 object_path_.c_str(), BLUEZ_DEVICE_INTERFACE,
                                 "Address", &err, &c_addr) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error getting address for device "
                       << object_path_ << " :" << err.message;
    return std::string();
  }
  
  std::string addr(c_addr);
  free(c_addr);
  return addr;
}
  
} // namespace linux
} // namespace nearby
