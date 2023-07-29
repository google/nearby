#include <cstring>

#include <memory>
#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/strings/str_replace.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

int bluez_interfaces_added_signal_handler(sd_bus_message *m, void *userdata,
                                          sd_bus_error *ret_error) {
  const sd_bus_error *reply_err = sd_bus_message_get_error(m);
  if (reply_err) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Received error while listening for InterfacesAdded: "
                       << reply_err->message;
    return 0;
  }

  struct BluetoothClassicMedium::DiscoveryParams *params =
      static_cast<BluetoothClassicMedium::DiscoveryParams *>(userdata);
  char *c_object_path = nullptr;
  int ret = sd_bus_message_read(m, "o", &c_object_path);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Error reading object path from message: " << ret;
    return ret;
  }

  std::string object_path(c_object_path);

  if (!absl::StrContains(object_path, absl::StrCat(params->adapter_object_path,
                                                   "/", "dev_"))) {
    // Interface added for an object we dont care about.
    return 0;
  }

  if (params->devices_by_path.count(object_path) != 0) {
    // Object already exists
    return 0;
  }

  ret = sd_bus_message_enter_container(m, 'a', "{sa{sv}}");
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error entering container: " << ret;
    return 0;
  }

  while (true) {
    const char *interface_name = nullptr;
    ret = sd_bus_message_read(m, "s", &interface_name);
    if (ret < 0) {
      NEARBY_LOGS(ERROR) << __func__ << "Error reading dict entry: " << ret;
    }
    if (ret == 0)
      break;

    if (strcmp(interface_name, "org.bluez.Device1") == 0) {
      NEARBY_LOGS(INFO) << __func__ << "Encountered new device at "
                        << c_object_path;
      auto bluetoothDevice =
          std::make_unique<BluetoothDevice>(BluetoothDevice(object_path));
      params->devices_by_path[object_path] = std::move(bluetoothDevice);

      if (params->cb.device_discovered_cb != nullptr) {
        params->cb.device_discovered_cb(*params->devices_by_path[object_path]);
      }
      for (auto &observer : params->observers_.GetObservers()) {
        observer->DeviceAdded(*params->devices_by_path[object_path]);
      }
      return 0;
    }
  }

  return 0;
}

BluetoothClassicMedium::BluetoothClassicMedium(absl::string_view adapter) {
  if (sd_bus_default_system(&system_bus_) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error connecting to system bus";
  }
  adapter_object_path_ = absl::Substitute("/org/bluez/$0/", adapter);
}

BluetoothClassicMedium::~BluetoothClassicMedium() {
  if (system_bus_)
    sd_bus_unref(system_bus_);
  if (system_bus_slot_)
    sd_bus_slot_unref(system_bus_slot_);
}

bool BluetoothClassicMedium::StartDiscovery(
    DiscoveryCallback discovery_callback) {
  if (!system_bus_)
    return false;

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  __attribute__((cleanup(sd_bus_message_unrefp))) sd_bus_message *reply =
      nullptr;

  discovery_params_.cb = std::move(discovery_callback);
  discovery_params_.adapter_object_path = adapter_object_path_;

  sd_bus_match_signal(system_bus_, &system_bus_slot_, BLUEZ_SERVICE, "/",
                      "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
                      bluez_interfaces_added_signal_handler,
                      &discovery_params_);

  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE,
                         adapter_object_path_.c_str(), BLUEZ_ADAPTER_INTERFACE,
                         "StartDiscovery", &err, &reply, nullptr) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error calling StartDiscovery on adapter "
                       << adapter_object_path_ << ": " << err.message;
    return false;
  }

  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  if (!system_bus_)
    return false;

  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  __attribute__((cleanup(sd_bus_message_unrefp))) sd_bus_message *reply =
      nullptr;

  int ret = sd_bus_call_method(
      system_bus_, BLUEZ_SERVICE, adapter_object_path_.c_str(),
      BLUEZ_ADAPTER_INTERFACE, "StopDiscovery", &err, &reply, nullptr);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error calling StopDiscovery on "
                       << adapter_object_path_ << ": " << err.message;
    return false;
  }
  const sd_bus_error *m_err = sd_bus_message_get_error(reply);

  if (m_err) {
    NEARBY_LOGS(ERROR) << __func__ << "Error calling StopDiscovery on "
                       << adapter_object_path_ << ": " << err.message;
    return false;
  }

  return true;
}

std::unique_ptr<api::BluetoothSocket>
BluetoothClassicMedium::ConnectToService(api::BluetoothDevice &remote_device,
                                         const std::string &service_uuid,
                                         CancellationFlag *cancellation_flag) {
  auto device_object_path = GetDeviceObjectPath(remote_device.GetMacAddress());
  
}

std::string
BluetoothClassicMedium::GetDeviceObjectPath(absl::string_view mac_address) {
  return absl::Substitute("$0/dev_$1", adapter_object_path_, absl::StrReplaceAll(mac_address, {{":", "_"}}));
}

} // namespace linux
} // namespace nearby
