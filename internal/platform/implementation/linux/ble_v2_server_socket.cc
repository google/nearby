// Copyright 2024 Google LLC
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

#include "internal/platform/implementation/linux/ble_v2_server_socket.h"

#include <memory>

#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_v2_socket.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

std::unique_ptr<api::ble_v2::BleSocket> BleV2ServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "BleV2ServerSocket::Accept waiting for connection";

  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }

  if (closed_) {
    LOG(INFO) << "BleV2ServerSocket::Accept socket is closed";
    return nullptr;
  }

  std::unique_ptr<BleV2Socket> socket = std::move(pending_sockets_.front());
  pending_sockets_.pop_front();

  LOG(INFO) << "BleV2ServerSocket::Accept accepted connection";
  return socket;
}

Exception BleV2ServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "BleV2ServerSocket::Close for service " << service_id_;

  if (closed_) {
    return {Exception::kSuccess};
  }

  closed_ = true;
  
  // Close all pending sockets
  for (auto& socket : pending_sockets_) {
    if (socket) {
      socket->Close();
    }
  }
  pending_sockets_.clear();
  
  cond_.SignalAll();

  return {Exception::kSuccess};
}

void BleV2ServerSocket::AddPendingSocket(std::unique_ptr<BleV2Socket> socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    LOG(WARNING)
        << "BleV2ServerSocket::AddPendingSocket socket is closed";
    return;
  }

  pending_sockets_.push_back(std::move(socket));
  cond_.SignalAll();
  LOG(INFO) << "BleV2ServerSocket::AddPendingSocket added socket, "
                    << "pending count: " << pending_sockets_.size();
}

}  // namespace linux
}  // namespace nearby
