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
  BluetoothSocket(api::BluetoothDevice &device,
                  absl::string_view device_object_path,
                  absl::string_view connected_profile_uuid, int fd)
      : device_(device) {
    fd_ = fd;
    device_object_path_ = device_object_path;
    connected_profile_uuid_ = connected_profile_uuid;
    input_stream_ = BluetoothInputStream(fd_);
    output_stream_ = BluetoothOutputStream(fd_);
  }

  InputStream &GetInputStream() override { return input_stream_; }
  OutputStream &GetOutputStream() override { return output_stream_; }
  Exception Close() override;
  api::BluetoothDevice *GetRemoteDevice() override { return &device_; };

private:
  int fd_;
  std::string device_object_path_;
  api::BluetoothDevice &device_;
  std::string connected_profile_uuid_;
  BluetoothInputStream input_stream_ = {-1};
  BluetoothOutputStream output_stream_ = {-1};
};
} // namespace linux
} // namespace nearby
#endif
