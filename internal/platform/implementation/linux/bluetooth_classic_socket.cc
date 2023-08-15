#include <array>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

#include <systemd/sd-bus.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

ExceptionOr<ByteArray> BluetoothInputStream::Read(std::int64_t size) {
  if (!fd_.has_value())
    return Exception::kIo;

  char *data = new char[size];
  ssize_t ret = read(fd_->get(), data, size);
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
  if (!fd_.has_value())
    return Exception::kIo;

  auto off = lseek(fd_->get(), offset, SEEK_CUR);
  if (off != offset) {
    auto end = lseek(fd_->get(), 0, SEEK_END);
    return off == end ? ExceptionOr((std::size_t)off) : Exception::kIo;
  }
  return ExceptionOr((std::size_t)(off));
}

ExceptionOr<ByteArray> BluetoothInputStream::ReadExactly(std::size_t size) {
  if (!fd_.has_value())
    return Exception::kIo;

  char *data = new char[size];
  ssize_t ret = read(fd_->get(), data, size);
  if (ret < 0) {
    delete[] data;
    return Exception::kIo;
  }

  return ExceptionOr(ByteArray(data, size));
}

Exception BluetoothOutputStream::Write(const ByteArray &data) {
  if (!fd_.has_value())
    return Exception{Exception::kIo};

  ssize_t written = 0;
  while (written < data.size()) {
    ssize_t ret = write(fd_->get(), data.data(), data.size());
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
  return close(fd_->get()) < 0 ? Exception{Exception::kIo}
                               : Exception{Exception::kSuccess};
}

Exception BluetoothSocket::Close() {
  input_stream_.fd_.reset();
  output_stream_.fd_.reset();

  return Exception{Exception::kSuccess};
}

} // namespace linux
} // namespace nearby
