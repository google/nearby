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

#include "internal/platform/implementation/linux/ble_v2_socket_adapter.h"

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/ble_v2_socket.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace linux {

// Standard UUIDs for Nearby Connections GATT socket service
// RX: Remote writes to this, we read from it
constexpr uint64_t kRxCharMsb = 0x0000FE2C00001000ULL;
constexpr uint64_t kRxCharLsb = 0x800000805F9B34FBULL;

// TX: We write to this (notify), remote reads from it
constexpr uint64_t kTxCharMsb = 0x0000FE2C00002000ULL;
constexpr uint64_t kTxCharLsb = 0x800000805F9B34FBULL;

Uuid BleV2SocketAdapter::GetRxCharacteristicUuid() {
  return Uuid(kRxCharMsb, kRxCharLsb);
}

Uuid BleV2SocketAdapter::GetTxCharacteristicUuid() {
  return Uuid(kTxCharMsb, kTxCharLsb);
}

void BleV2SocketAdapter::RegisterSocket(
    api::ble_v2::BlePeripheral::UniqueId device_id, BleV2Socket* socket) {
  absl::MutexLock lock(&mutex_);
  device_sockets_[device_id] = socket;
  LOG(INFO) << "Registered socket for device " << device_id;
}

void BleV2SocketAdapter::UnregisterSocket(
    api::ble_v2::BlePeripheral::UniqueId device_id) {
  absl::MutexLock lock(&mutex_);
  device_sockets_.erase(device_id);
  LOG(INFO) << "Unregistered socket for device " << device_id;
}

api::ble_v2::ServerGattConnectionCallback
BleV2SocketAdapter::CreateServerCallbacks() {
  api::ble_v2::ServerGattConnectionCallback callbacks;

  // Handle subscription to TX characteristic (remote wants to receive data)
  callbacks.characteristic_subscription_cb =
      [](const api::ble_v2::GattCharacteristic& characteristic) {
        LOG(INFO) << "Remote subscribed to characteristic: "
                          << std::string(characteristic.uuid);
      };

  // Handle unsubscription
  callbacks.characteristic_unsubscription_cb =
      [](const api::ble_v2::GattCharacteristic& characteristic) {
        LOG(INFO) << "Remote unsubscribed from characteristic: "
                          << std::string(characteristic.uuid);
      };

  // Handle read requests (not typically used for socket data transfer)
  callbacks.on_characteristic_read_cb =
      [](api::ble_v2::BlePeripheral::UniqueId remote_device_id,
         const api::ble_v2::GattCharacteristic& characteristic, int offset,
         api::ble_v2::ServerGattConnectionCallback::ReadValueCallback
             callback) {
        LOG(INFO) << "Read request from device " << remote_device_id
                          << " on characteristic "
                          << std::string(characteristic.uuid);
        // Return empty data for reads
        callback(absl::string_view(""));
      };

  // Handle write requests - THIS IS WHERE DATA COMES IN
  callbacks.on_characteristic_write_cb =
      [this](api::ble_v2::BlePeripheral::UniqueId remote_device_id,
             const api::ble_v2::GattCharacteristic& characteristic, int offset,
             absl::string_view data,
             api::ble_v2::ServerGattConnectionCallback::WriteValueCallback
                 callback) {
        LOG(INFO) << "Write request from device " << remote_device_id
                          << " on characteristic "
                          << std::string(characteristic.uuid) << ", data size: "
                          << data.size();

        // Find the socket for this device
        BleV2Socket* socket = nullptr;
        {
          absl::MutexLock lock(&mutex_);
          auto it = device_sockets_.find(remote_device_id);
          if (it != device_sockets_.end()) {
            socket = it->second;
          }
        }

        if (socket) {
          // Route the data to the socket's input stream
          ByteArray byte_data(data.data(), data.size());
          socket->ReceiveData(byte_data);
          callback(absl::OkStatus());
          LOG(INFO) << "Routed " << data.size()
                            << " bytes to socket input stream";
        } else {
          LOG(WARNING) << "No socket registered for device "
                               << remote_device_id;
          callback(absl::NotFoundError("No socket for device"));
        }
      };

  return callbacks;
}

}  // namespace linux
}  // namespace nearby
