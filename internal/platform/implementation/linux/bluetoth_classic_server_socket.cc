#include "absl/strings/str_replace.h"
#include "absl/strings/substitute.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_server_socket.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"
#include <systemd/sd-bus.h>

namespace nearby {
namespace linux {
std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  auto pair = profile_manager_.GetServiceRecordFD(service_uuid_);
  if (!pair.has_value()) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Failed to get a new connection for profile "
                       << service_uuid_ << " for device ";
    return nullptr;
  }
  auto device_object_path =
      absl::Substitute("$0/dev_$1", adapter_object_path_,
                       absl::StrReplaceAll(pair->first, {{":", "_"}}));
  auto device = BluetoothDevice(sd_bus_ref(system_bus_), device_object_path);
  return std::unique_ptr<api::BluetoothSocket>(new BluetoothSocket(
      device, device_object_path, service_uuid_, pair->second));
}

Exception BluetoothServerSocket::Close() {
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;
  auto profile_object_path =
      absl::Substitute("/com/github/google/nearby/profiles/$0", service_uuid_);

  if (sd_bus_call_method(system_bus_, BLUEZ_SERVICE, "/org/bluez",
                         "org.bluez.ProfileManager1", "UnregisterProfile", &err,
                         nullptr, "o", profile_object_path.c_str()) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error unregistering profile object "
                       << profile_object_path << ": " << err.message;
    return {Exception::kFailed};
  }

  return {Exception::kSuccess};
}
} // namespace linux
} // namespace nearby
