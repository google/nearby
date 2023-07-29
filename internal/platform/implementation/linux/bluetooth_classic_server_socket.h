#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SERVER_SOCKET_H_

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace linux {
class BluetoothServerSocket : api::BluetoothServerSocket {
public:
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
};
} // namespace linux
} // namespace nearby

#endif
