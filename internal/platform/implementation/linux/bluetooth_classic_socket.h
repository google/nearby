#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_

#include <memory>

#include <systemd/sd-bus.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace linux {

class BluetoothInputStream : public InputStream {
public:
  BluetoothInputStream(int fd) { fd_ = fd; };

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  ExceptionOr<size_t> Skip(size_t offset) override;
  ExceptionOr<ByteArray> ReadExactly(std::size_t size);

  Exception Close() override;

private:
  int fd_;
};

class BluetoothOutputStream : public OutputStream {
public:
  BluetoothOutputStream(int fd) { fd_ = fd; };

  Exception Write(const ByteArray &data) override;
  Exception Flush() override;
  Exception Close() override;

private:
  int fd_;
};

class BluetoothSocket : public api::BluetoothSocket {
public:
  BluetoothSocket(std::string object, int fd) {
    fd_ = fd;
    object_ = object;
    input_stream_ = BluetoothInputStream(fd_);
    output_stream_ = BluetoothOutputStream(fd_);
  }

  InputStream &GetInputStream() override { return input_stream_; }
  OutputStream &GetOutputStream() override { return output_stream_; }

private:
  int fd_;
  std::string object_;
  BluetoothInputStream input_stream_ = {-1};
  BluetoothOutputStream output_stream_ = {-1};
};
} // namespace linux
} // namespace nearby
#endif
