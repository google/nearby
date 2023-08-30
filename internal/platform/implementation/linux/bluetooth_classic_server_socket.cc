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
#include "absl/strings/str_replace.h"
#include "absl/strings/substitute.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  auto pair = profile_manager_.GetServiceRecordFD(service_uuid_);
  if (!pair.has_value()) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Failed to get a new connection for profile "
                       << service_uuid_ << " for device ";
    return nullptr;
  }

  auto [device, fd] = *pair;
  return std::unique_ptr<api::BluetoothSocket>(new BluetoothSocket(device, fd));
}

Exception BluetoothServerSocket::Close() {
  auto profile_object_path =
      absl::Substitute("/com/google/nearby/profiles/$0", service_uuid_);

  profile_manager_.Unregister(service_uuid_);

  return {Exception::kSuccess};
}
}  // namespace linux
}  // namespace nearby
