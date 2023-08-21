#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_

#include <memory>
#include <optional>

#include <sdbus-c++/Types.h>
#include <systemd/sd-bus.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/stream.h"

namespace nearby {
namespace linux {
class BluetoothSocket : public api::BluetoothSocket {
public:
  BluetoothSocket(api::BluetoothDevice &device, sdbus::UnixFd fd)
      : device_(device), output_stream_(fd), input_stream_(fd) {}

  nearby::InputStream &GetInputStream() override { return input_stream_; }
  nearby::OutputStream &GetOutputStream() override { return output_stream_; }
  Exception Close() override;
  api::BluetoothDevice *GetRemoteDevice() override { return &device_; };

private:
  api::BluetoothDevice &device_;
  OutputStream output_stream_;
  InputStream input_stream_;
};
} // namespace linux
} // namespace nearby
#endif
