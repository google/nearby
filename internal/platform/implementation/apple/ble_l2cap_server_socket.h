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

#include <memory>

#include "absl/functional/any_invocable.h"
#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/ble_l2cap_socket.h"
#include "internal/platform/implementation/ble.h"

namespace nearby {
namespace apple {

// A BLE L2CAP server socket for listening incoming L2CAP socket.
class BleL2capServerSocket : public api::ble::BleL2capServerSocket {
 public:
  BleL2capServerSocket() = default;
  ~BleL2capServerSocket() override;

  // Gets PSM value has been published by the server.
  int GetPSM() const override;

  // Sets PSM value has been published by the server.
  void SetPSM(int psm);

  // Wait for an available socket.
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and L2CAP ServerSocket has to be
  // closed.
  std::unique_ptr<api::ble::BleL2capSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes the L2CAP server socket.
  Exception Close() override;

  // Adds a pending socket to the server socket.
  bool AddPendingSocket(std::unique_ptr<BleL2capSocket> socket);

  // Called by the server side of a connection before accessing the server
  // socket pointer at the callback of l2cap channel creation, to track validity
  // of a pointer to this server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;
  absl::flat_hash_set<std::unique_ptr<BleL2capSocket>> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  int psm_ = 0;
};

}  // namespace apple
}  // namespace nearby