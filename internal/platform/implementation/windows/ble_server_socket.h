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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SERVER_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SERVER_SOCKET_H_

#include <deque>
#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/windows/ble_socket.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"

namespace nearby {
namespace windows {

class BleServerSocket : public api::ble::BleServerSocket {
 public:
  explicit BleServerSocket(api::BluetoothAdapter* adapter);
  ~BleServerSocket() override = default;

  std::unique_ptr<::nearby::api::ble::BleSocket> Accept() override;

  Exception Close() override;

  // Create GATT service and send/receive characteristic.
  bool Bind();

 private:
  BluetoothAdapter* adapter_ = nullptr;

  // Accept notification.
  absl::Mutex mutex_;
  absl::CondVar cond_;
  bool closed_ = false;
  std::deque<BleSocket> pending_sockets_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_SERVER_SOCKET_H_
