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
#include <systemd/sd-bus.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/stream.h"

namespace nearby {
namespace linux {
class BluetoothSocket final : public api::BluetoothSocket {
 public:
  BluetoothSocket(api::BluetoothDevice &device, sdbus::UnixFd fd)
      : device_(device), output_stream_(fd), input_stream_(fd) {}

  nearby::InputStream &GetInputStream() override { return input_stream_; }
  nearby::OutputStream &GetOutputStream() override { return output_stream_; }
  Exception Close() override {
    input_stream_.Close();
    output_stream_.Close();

    return Exception{Exception::kSuccess};
  }
  api::BluetoothDevice *GetRemoteDevice() override { return &device_; };

 private:
  api::BluetoothDevice &device_;
  OutputStream output_stream_;
  InputStream input_stream_;
};
}  // namespace linux
}  // namespace nearby
#endif
