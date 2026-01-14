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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_ADAPTER_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_ADAPTER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_v2_socket.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {

// Helper class to adapt GATT server callbacks to socket data streams
// This wires up the ServerGattConnectionCallback to feed data into BleV2Socket
class BleV2SocketAdapter {
 public:
  BleV2SocketAdapter() = default;
  ~BleV2SocketAdapter() = default;

  // Create GATT server callbacks that will route data to/from sockets
  api::ble_v2::ServerGattConnectionCallback CreateServerCallbacks();

  // Register a socket for a specific remote device
  // When GATT writes come from this device, data is routed to this socket
  void RegisterSocket(api::ble_v2::BlePeripheral::UniqueId device_id,
                     BleV2Socket* socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Unregister a socket
  void UnregisterSocket(api::ble_v2::BlePeripheral::UniqueId device_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Get the RX characteristic UUID (for receiving data from remote)
  static Uuid GetRxCharacteristicUuid();

  // Get the TX characteristic UUID (for sending data to remote)
  static Uuid GetTxCharacteristicUuid();

 private:
  absl::Mutex mutex_;
  // Map of device ID to socket for routing incoming GATT writes
  absl::flat_hash_map<api::ble_v2::BlePeripheral::UniqueId, BleV2Socket*>
      device_sockets_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_ADAPTER_H_
