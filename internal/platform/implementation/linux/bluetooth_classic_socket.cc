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

#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
Exception Poller::Ready() {
  while (true) {
    auto ret = poll(fds_, 1, -1);
    if (ret < 0) {
      if (errno == EAGAIN) continue;
      LOG(ERROR) << __func__ << ": error polling socket for I/O: "
                         << std::strerror(errno);
      return {Exception::kIo};
    }
    if ((fds_[0].revents & poll_event_) != 0) {
      return {Exception::kSuccess};
    }
    if ((fds_[0].revents & POLLHUP) != 0) {
      LOG(ERROR) << __func__ << ": socket disconnected";
      return {Exception::kIo};
    }
    if ((fds_[0].revents & (POLLERR | POLLNVAL)) != 0) {
      LOG(ERROR) << __func__ << ": an error occured on the socket";
      return {Exception::kIo};
    }
  }
}

ExceptionOr<ByteArray> BluetoothInputStream::Read(std::int64_t size) {
  // Check if FD is valid before proceeding
  {
    absl::MutexLock lock(&fd_mutex_);
    if (!fd_.isValid()) return Exception{Exception::kIo};
  }

  // Create poller while we still have the FD (copy the fd value)
  auto poller = Poller::CreateInputPoller(fd_);

  std::string buffer;
  buffer.resize(size);
  char *data = buffer.data();

  size_t total_read = 0;

  while (total_read < size) {
    auto result = poller.Ready();
    if (result.Raised()) return result;

    auto bytes_read = read(fd_.get(), &data[total_read], (size - total_read));
    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
      if (errno == EBADF) {
        // FD was closed by another thread
        LOG(INFO) << __func__ << ": socket was closed during read";
        return {Exception::kIo};
      }
      LOG(ERROR) << __func__
                         << ": error reading data on bluetooth socket: "
                         << std::strerror(errno);
      return {Exception::kIo};
    }
    if (bytes_read == 0) {
      // EOF - socket closed
      LOG(INFO) << __func__ << ": socket closed (EOF)";
      return {Exception::kIo};
    }
    total_read += bytes_read;
  }

  return ExceptionOr{ByteArray(std::move(buffer))};
}

Exception BluetoothInputStream::Close() {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return {Exception::kSuccess};  // Already closed
  fd_.reset();
  return {Exception::kSuccess};
}

Exception BluetoothOutputStream::Write(const ByteArray &data) {
  // Check if FD is valid before proceeding
  {
    absl::MutexLock lock(&fd_mutex_);
    if (!fd_.isValid()) return Exception{Exception::kIo};
  }

  // Create poller while we still have the FD (copy the fd value)
  auto poller = Poller::CreateOutputPoller(fd_);

  size_t total_wrote = 0;

  while (total_wrote < data.size()) {
    auto result = poller.Ready();
    if (result.Raised()) return result;

    const char *buf = data.data();
    auto wrote =
        write(fd_.get(), &buf[total_wrote], (data.size() - total_wrote));
    if (wrote < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
      if (errno == EBADF || errno == EPIPE) {
        // FD was closed by another thread
        LOG(INFO) << __func__ << ": socket was closed during write";
        return {Exception::kIo};
      }
      LOG(ERROR) << __func__
                         << ": error writing data on bluetooth socket: "
                         << std::strerror(errno);
      return {Exception::kIo};
    }

    total_wrote += wrote;
  }

  return {Exception::kSuccess};
}

Exception BluetoothOutputStream::Close() {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return {Exception::kSuccess};  // Already closed
  fd_.reset();
  return {Exception::kSuccess};
}

}  // namespace linux
}  // namespace nearby
