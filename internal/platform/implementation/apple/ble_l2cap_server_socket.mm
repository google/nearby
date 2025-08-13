// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/ble_l2cap_server_socket.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"

namespace nearby {
namespace apple {

BleL2capServerSocket::~BleL2capServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

int BleL2capServerSocket::GetPSM() const { return psm_; }

void BleL2capServerSocket::SetPSM(int psm) { psm_ = psm; }

// TODO: b/399815436 - Refactor Accept() and AddPendingSocket() for better readability.
std::unique_ptr<api::ble_v2::BleL2capSocket> BleL2capServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  if (closed_) return {};

  std::unique_ptr<BleL2capSocket> remote_socket =
      std::move(pending_sockets_.extract(pending_sockets_.begin()).value());
  return std::move(remote_socket);
}

bool BleL2capServerSocket::AddPendingSocket(std::unique_ptr<BleL2capSocket> socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return false;
  }
  pending_sockets_.insert(std::move(socket));
  cond_.SignalAll();
  return !closed_;
}

void BleL2capServerSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

Exception BleL2capServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception BleL2capServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.Unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.Lock();
    }
  }
  return {Exception::kSuccess};
}

}  // namespace apple
}  // namespace nearby