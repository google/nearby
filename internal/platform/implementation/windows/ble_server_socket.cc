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

#include "internal/platform/implementation/windows/ble_server_socket.h"

#include <cstddef>
#include <exception>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble_socket.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

BleServerSocket::BleServerSocket(api::BluetoothAdapter* adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(adapter)) {
  DCHECK_NE(adapter_, nullptr);
}

std::unique_ptr<api::ble::BleSocket> BleServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  VLOG(1) << __func__ << ": Accept is called.";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return nullptr;

  BleSocket ble_socket = pending_sockets_.front();
  pending_sockets_.pop_front();

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<BleSocket>(ble_socket);
}

Exception BleServerSocket::Close() {
  // TODO(b/271031645): implement BLE socket using weave
  absl::MutexLock lock(&mutex_);
  VLOG(1) << __func__ << ": Close is called.";

  if (closed_) {
    return {Exception::kSuccess};
  }

  closed_ = true;
  cond_.SignalAll();

  return {Exception::kSuccess};
}

bool BleServerSocket::Bind() {
  // TODO(b/271031645): implement BLE socket using weave
  LOG(ERROR) << __func__ << ": GATT socket started.";
  return true;
}

}  // namespace windows
}  // namespace nearby
