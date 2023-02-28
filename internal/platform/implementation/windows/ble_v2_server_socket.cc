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

#include "internal/platform/implementation/windows/ble_v2_server_socket.h"

#include <cstddef>
#include <exception>
#include <memory>

#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/ble_v2_socket.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

BleV2ServerSocket::BleV2ServerSocket(api::BluetoothAdapter* adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(adapter)) {
  DCHECK_NE(adapter_, nullptr);
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2ServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return nullptr;

  BleV2Socket ble_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  NEARBY_LOGS(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<BleV2Socket>(ble_socket);
}

Exception BleV2ServerSocket::Close() {
  // TODO(b/271031645): implement BLE socket using weave
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Close is called.";

  if (closed_) {
    return {Exception::kSuccess};
  }

  closed_ = true;
  cond_.SignalAll();

  return {Exception::kSuccess};
}

bool BleV2ServerSocket::Bind() {
  // TODO(b/271031645): implement BLE socket using weave
  NEARBY_LOGS(ERROR) << __func__ << ": GATT socket started.";
  return true;
}

}  // namespace windows
}  // namespace nearby
