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

#ifndef PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_H_
#define PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_H_

#include <cstdint>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {

// BLE v2 Socket implementation using GATT characteristics for data transfer
// 
// Data flow:
// - Server side:
//   * RX characteristic: Remote writes -> our InputStream reads
//   * TX characteristic: Our OutputStream writes -> remote reads via notifications
// - Client side:
//   * TX characteristic: Our OutputStream writes -> remote reads  
//   * RX characteristic: Remote writes (notifications) -> our InputStream reads
class BleV2Socket : public api::ble_v2::BleSocket {
 public:
  BleV2Socket() = default;
  explicit BleV2Socket(api::ble_v2::BlePeripheral::UniqueId peripheral_id)
      : peripheral_id_(peripheral_id) {}
  ~BleV2Socket() override { Close(); }

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  api::ble_v2::BlePeripheral::UniqueId GetRemotePeripheralId() override {
    return peripheral_id_;
  }

  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

  // GATT integration: Allow external code to feed data to input stream
  // Called when remote device writes to RX characteristic
  void ReceiveData(const ByteArray& data) { input_stream_.ReceiveData(data); }

  // GATT integration: Set callback for output stream writes
  // Callback should write to TX characteristic (notify remote)
  void SetWriteCallback(
      absl::AnyInvocable<bool(const ByteArray& data)> callback) {
    output_stream_.SetWriteCallback(std::move(callback));
  }

  // Set the GATT server and characteristics for server-side socket
  void SetGattServer(std::unique_ptr<api::ble_v2::GattServer> gatt_server,
                     const api::ble_v2::GattCharacteristic& rx_char,
                     const api::ble_v2::GattCharacteristic& tx_char)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Set the GATT client and characteristics for client-side socket
  void SetGattClient(std::unique_ptr<api::ble_v2::GattClient> gatt_client,
                     const api::ble_v2::GattCharacteristic& rx_char,
                     const api::ble_v2::GattCharacteristic& tx_char)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  class BleInputStream : public InputStream {
   public:
    BleInputStream() = default;
    ~BleInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override
        ABSL_LOCKS_EXCLUDED(mutex_);
    Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

    void ReceiveData(const ByteArray& data) ABSL_LOCKS_EXCLUDED(mutex_);
    void NotifyClose() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    absl::Mutex mutex_;
    absl::CondVar cond_;
    ByteArray buffer_ ABSL_GUARDED_BY(mutex_);
    bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  };

  class BleOutputStream : public OutputStream {
   public:
    BleOutputStream() = default;
    ~BleOutputStream() override = default;

    Exception Write(const ByteArray& data) override
        ABSL_LOCKS_EXCLUDED(mutex_);
    Exception Flush() override;
    Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

    using WriteCallback = absl::AnyInvocable<bool(const ByteArray& data)>;
    void SetWriteCallback(WriteCallback callback)
        ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    absl::Mutex mutex_;
    WriteCallback write_callback_ ABSL_GUARDED_BY(mutex_);
    bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  };

  mutable absl::Mutex mutex_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  BleInputStream input_stream_;
  BleOutputStream output_stream_;
  api::ble_v2::BlePeripheral::UniqueId peripheral_id_ = 0;

  // GATT resources (only one of these will be set)
  std::unique_ptr<api::ble_v2::GattServer> gatt_server_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<api::ble_v2::GattClient> gatt_client_ ABSL_GUARDED_BY(mutex_);
  
  // Characteristics for data transfer
  api::ble_v2::GattCharacteristic rx_char_ ABSL_GUARDED_BY(mutex_);
  api::ble_v2::GattCharacteristic tx_char_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_API_BLE_V2_SOCKET_H_
