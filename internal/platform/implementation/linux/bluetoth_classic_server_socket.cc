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

  auto [device, fd] = *pair;
  return std::unique_ptr<api::BluetoothSocket>(new BluetoothSocket(device, fd));
}

Exception BluetoothServerSocket::Close() {
  auto profile_object_path =
      absl::Substitute("/com/github/google/nearby/profiles/$0", service_uuid_);

  profile_manager_.Unregister(service_uuid_);

  return {Exception::kSuccess};
}
} // namespace linux
} // namespace nearby
