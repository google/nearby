#include <array>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"

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
} // namespace linux
} // namespace nearby
