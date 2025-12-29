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

#include "internal/platform/implementation/linux/bluetooth_classic_server_socket.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  if (stopped_.Cancelled()) {
    LOG(ERROR) << __func__ << ": server socket has been stopped";
    return nullptr;
  }

  LOG(INFO) << __func__
                       << ": accepting new connections for service uuid "
                       << service_uuid_;

  auto pair = profile_manager_.GetServiceRecordFD(service_uuid_, &stopped_);
  if (!pair.has_value()) {
    if (!stopped_.Cancelled())
      LOG(ERROR) << __func__
                         << ": Failed to get a new connection for profile "
                         << service_uuid_;
    return nullptr;
  }

  auto [device, fd] = *pair;
  return std::make_unique<BluetoothSocket>(device, std::move(fd));
}

Exception BluetoothServerSocket::Close() {
  LOG(ERROR) << __func__ << ": closing bluetooth server socket";
  stopped_.Cancel();
  profile_manager_.Unregister(service_uuid_);

  return {Exception::kSuccess};
}
}  // namespace linux
}  // namespace nearby
