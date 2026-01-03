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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_SOCKET_H_

#include <memory>
#include <optional>

#include <sdbus-c++/Types.h>
#include <sys/poll.h>
#include <systemd/sd-bus.h>

#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {
// BlueZ's NewConnection gives us a non-blocking FD, so we need to poll
// it to be able to write/read bytes.
class Poller final {
 public:
  static Poller CreateInputPoller(const sdbus::UnixFd &fd) {
    return Poller(fd, POLLIN);
  }

  static Poller CreateOutputPoller(const sdbus::UnixFd &fd) {
    return Poller(fd, POLLOUT);
  }

  Exception Ready();

 private:
  Poller(const sdbus::UnixFd &fd, short event) : poll_event_(event) {
    fds_[0].fd = fd.get();
    fds_[0].events = event;
  }

  short poll_event_;
  struct pollfd fds_[1];
};

class BluetoothInputStream final : public nearby::InputStream {
 public:
  explicit BluetoothInputStream(sdbus::UnixFd fd) : fd_(std::move(fd)){};

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  mutable absl::Mutex fd_mutex_;
  sdbus::UnixFd fd_ ABSL_GUARDED_BY(fd_mutex_);
};

class BluetoothOutputStream : public nearby::OutputStream {
 public:
  explicit BluetoothOutputStream(sdbus::UnixFd fd) : fd_(std::move(fd)){};

  Exception Write(const ByteArray &data) override;
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override;

 private:
  mutable absl::Mutex fd_mutex_;
  sdbus::UnixFd fd_ ABSL_GUARDED_BY(fd_mutex_);
};

class BluetoothSocket final : public api::BluetoothSocket {
 public:
  BluetoothSocket(std::shared_ptr<BluetoothDevice> device,
                  const sdbus::UnixFd &fd)
      : device_(std::move(device)), output_stream_(fd), input_stream_(fd) {}

  nearby::InputStream &GetInputStream() override { return input_stream_; }
  nearby::OutputStream &GetOutputStream() override { return output_stream_; }
  Exception Close() override {
    input_stream_.Close();
    output_stream_.Close();

    return Exception{Exception::kSuccess};
  }
  api::BluetoothDevice *GetRemoteDevice() override { return device_.get(); };

 private:
  std::shared_ptr<BluetoothDevice> device_;
  BluetoothOutputStream output_stream_;
  BluetoothInputStream input_stream_;
};
}  // namespace linux
}  // namespace nearby
#endif
