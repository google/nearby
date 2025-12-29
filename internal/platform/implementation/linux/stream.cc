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

#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstdint>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

ExceptionOr<ByteArray> InputStream::Read(std::int64_t size) {
  if (!fd_.isValid()) return {Exception::kIo};

  std::string buffer;
  buffer.resize(size);
  ssize_t ret = recv(fd_.get(), buffer.data(), buffer.size(), MSG_WAITALL);
  if (ret == 0) {
    return ExceptionOr(ByteArray());
  }
  if (ret < 0) {
    LOG(ERROR) << __func__
                       << ": error reading from fd: " << std::strerror(errno);
    return {Exception::kIo};
  }
  buffer.resize(ret);

  return ExceptionOr(ByteArray(std::move(buffer)));
}

Exception InputStream::Close() {
  if (!fd_.isValid()) return Exception{Exception::kIo};
  fd_.reset();
  return {};
}

Exception OutputStream::Write(const ByteArray &data) {
  if (!fd_.isValid()) return Exception{Exception::kIo};

  size_t written = 0;
  while (written < data.size()) {
    ssize_t ret = write(fd_.get(), data.data(), data.size());
    if (ret < 0) {
      LOG(ERROR) << __func__
                         << ": error writing to fd: " << std::strerror(errno);
      return Exception{Exception::kIo};
    }
    written += ret;
  }
  return Exception{Exception::kSuccess};
}

Exception OutputStream::Flush() { return Exception{Exception::kSuccess}; }

Exception OutputStream::Close() {
  if (!fd_.isValid()) return Exception{Exception::kIo};

  auto ret = close(fd_.get()) < 0 ? Exception{Exception::kIo}
                                  : Exception{Exception::kSuccess};
  fd_.reset();
  return ret;
}

}  // namespace linux
}  // namespace nearby
