#include <array>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

#include <systemd/sd-bus.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

ExceptionOr<ByteArray> BluetoothInputStream::Read(std::int64_t size) {
  char *data = new char[size];
  ssize_t ret = read(fd_, data, size);
  if (ret == 0) {
    delete[] data;
    return ExceptionOr(ByteArray());
  } else if (ret < 0) {
    delete[] data;
    return Exception::kIo;
  }

  return ExceptionOr(ByteArray(data, size));
}

ExceptionOr<std::size_t> BluetoothInputStream::Skip(std::size_t offset) {
  auto off = lseek(fd_, offset, SEEK_CUR);
  if (off != offset) {
    auto end = lseek(fd_, 0, SEEK_END);
    return off == end ? ExceptionOr((std::size_t)off) : Exception::kIo;
  }
  return ExceptionOr((std::size_t)(off));
}

ExceptionOr<ByteArray> BluetoothInputStream::ReadExactly(std::size_t size) {
  char *data = new char[size];
  ssize_t ret = read(fd_, data, size);
  if (ret < 0) {
    delete[] data;
    return Exception::kIo;
  }

  return ExceptionOr(ByteArray(data, size));
}

Exception BluetoothOutputStream::Write(const ByteArray &data) {
  ssize_t written = 0;
  while (written < data.size()) {
    ssize_t ret = write(fd_, data.data(), data.size());
    if (ret < 1) {
      return Exception{Exception::kIo};
    }
    written += ret;
  }
  return Exception{Exception::kSuccess};
}

Exception BluetoothOutputStream::Flush() {
  return Exception{Exception::kSuccess};
}

Exception BluetoothOutputStream::Close() {
  return close(fd_) < 0 ? Exception{Exception::kIo}
                        : Exception{Exception::kSuccess};
}

Exception BluetoothSocket::Close() {
  __attribute__((cleanup(sd_bus_unrefp))) sd_bus *system_bus = NULL;
  __attribute__((cleanup(sd_bus_error_free))) sd_bus_error err =
      SD_BUS_ERROR_NULL;

  if (auto ret = sd_bus_default_system(&system_bus); ret < 0) {
    sd_bus_error_set_errno(&err, ret);
    NEARBY_LOGS(ERROR) << __func__
                       << "Error connecting to system bus: " << err.name << ": "
                       << err.message;
    return Exception{Exception::kFailed};
  }

  if (sd_bus_call_method(system_bus, BLUEZ_SERVICE, device_object_path_.c_str(),
                         BLUEZ_DEVICE_INTERFACE, "DisconnectProfile", &err,
                         nullptr, "s", connected_profile_uuid_.c_str()) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << "Error disconnecting from profile "
                       << connected_profile_uuid_ << " on device "
                       << device_object_path_ << ": " << err.name << ": "
                       << err.message;
    return Exception{Exception::kFailed};
  }

  return Exception{Exception::kSuccess};
}

} // namespace linux
} // namespace nearby
