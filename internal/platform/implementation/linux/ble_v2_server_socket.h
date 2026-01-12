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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_SERVER_SOCKET_H_

#include <deque>
#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_v2_socket.h"

namespace nearby {
namespace linux {

class BleV2ServerSocket final : public api::ble_v2::BleServerSocket {
 public:
  explicit BleV2ServerSocket(const std::string& service_id)
      : service_id_(service_id) {}
  ~BleV2ServerSocket() override = default;

  std::unique_ptr<api::ble_v2::BleSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  void AddPendingSocket(std::unique_ptr<BleV2Socket> socket)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::string GetServiceId() const { return service_id_; }

 private:
  std::string service_id_;
  absl::Mutex mutex_;
  absl::CondVar cond_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  std::deque<std::unique_ptr<BleV2Socket>> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif
