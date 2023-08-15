#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_

#include <memory>
#include <optional>

#include <sdbus-c++/Types.h>
#include <systemd/sd-bus.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {

class BluetoothInputStream : public InputStream {
public:
  BluetoothInputStream(sdbus::UnixFd &fd) : fd_(fd){};

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  ExceptionOr<size_t> Skip(size_t offset) override;
  ExceptionOr<ByteArray> ReadExactly(std::size_t size);

  Exception Close() override;

private:
  friend class BluetoothSocket;

  std::optional<sdbus::UnixFd> fd_;
};

class BluetoothOutputStream : public OutputStream {
public:
  BluetoothOutputStream(sdbus::UnixFd &fd) : fd_(fd){};

  Exception Write(const ByteArray &data) override;
  Exception Flush() override;
  Exception Close() override;

private:
  friend class BluetoothSocket;

  std::optional<sdbus::UnixFd> fd_;
};

class BluetoothSocket : public api::BluetoothSocket {
public:
  BluetoothSocket(api::BluetoothDevice &device, sdbus::UnixFd fd)
      : device_(device), output_stream_(fd), input_stream_(fd) {}

  InputStream &GetInputStream() override { return input_stream_; }
  OutputStream &GetOutputStream() override { return output_stream_; }
  Exception Close() override;
  api::BluetoothDevice *GetRemoteDevice() override { return &device_; };

private:
  api::BluetoothDevice &device_;
  BluetoothOutputStream output_stream_;
  BluetoothInputStream input_stream_;
};
} // namespace linux
} // namespace nearby
#endif
