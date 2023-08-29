// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <array>
#include <cerrno>
#include <cstdint>
#include <unistd.h>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"

namespace nearby {
namespace linux {

ExceptionOr<ByteArray> InputStream::Read(std::int64_t size) {
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

Exception InputStream::Close() {
  if (!fd_.has_value())
    return Exception{Exception::kIo};

  auto ret = close(fd_->get()) < 0 ? Exception{Exception::kIo}
                                   : Exception{Exception::kSuccess};
  fd_.reset();
  return ret;
}

Exception OutputStream::Write(const ByteArray &data) {
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

Exception OutputStream::Flush() { return Exception{Exception::kSuccess}; }

Exception OutputStream::Close() {
  if (!fd_.has_value())
    return Exception{Exception::kIo};

  auto ret = close(fd_->get()) < 0 ? Exception{Exception::kIo}
                                   : Exception{Exception::kSuccess};
  fd_.reset();
  return ret;
}

Exception BluetoothSocket::Close() {
  input_stream_.Close();
  output_stream_.Close();

  return Exception{Exception::kSuccess};
}

} // namespace linux
} // namespace nearby
