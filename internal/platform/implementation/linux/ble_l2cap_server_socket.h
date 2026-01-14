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

#ifndef PLATFORM_IMPL_LINUX_BLE_L2CAP_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLE_L2CAP_SERVER_SOCKET_H_

#include <memory>
#include <sdbus-c++/Types.h>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_l2cap_socket.h"
#include "absl/container/flat_hash_map.h"
namespace nearby {
namespace linux {

class BleL2capServerSocket final : public api::ble_v2::BleL2capServerSocket {
 public:
  BleL2capServerSocket();
  explicit BleL2capServerSocket(int psm);
  ~BleL2capServerSocket() override;

  int GetPSM() const override { return psm_; }
  void SetPSM(int psm);

  std::unique_ptr<api::ble_v2::BleL2capSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  int psm_ = 0;
  int server_fd_ ABSL_GUARDED_BY(mutex_) = -1;

  CancellationFlag stopped_;
  // <server_fd, <client_fd, peripheral_id>>
  absl::flat_hash_map<int, std::pair<int,api::ble_v2::BlePeripheral::UniqueId>> accepted_fds_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_BLE_L2CAP_SERVER_SOCKET_H_
