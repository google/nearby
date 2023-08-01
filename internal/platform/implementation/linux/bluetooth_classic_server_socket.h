#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_

#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_bluez_profile.h"

namespace nearby {
namespace linux {
class BluetoothServerSocket : public api::BluetoothServerSocket {
public:
  BluetoothServerSocket(sd_bus *system_bus, ProfileManager &profile_manager,
                        absl::string_view adapter_object_path,
                        absl::string_view service_uuid)
      : profile_manager_(profile_manager) {
    system_bus_ = system_bus;
    adapter_object_path_ = adapter_object_path;
    service_uuid_ = service_uuid;
  }
  ~BluetoothServerSocket() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be
  // closed.
  std::unique_ptr<api::BluetoothSocket> Accept() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

private:
  sd_bus *system_bus_;
  ProfileManager &profile_manager_;
  std::string adapter_object_path_;
  std::string service_uuid_;
};
} // namespace linux
} // namespace nearby

#endif
