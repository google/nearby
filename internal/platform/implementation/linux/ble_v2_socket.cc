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

#include "internal/platform/implementation/linux/ble_v2_socket.h"

#include <cstdint>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

InputStream& BleV2Socket::GetInputStream() { return input_stream_; }

OutputStream& BleV2Socket::GetOutputStream() { return output_stream_; }

Exception BleV2Socket::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }
  closed_ = true;
  
  // Close streams
  input_stream_.NotifyClose();
  output_stream_.Close();
  
  // Cleanup GATT resources
  if (gatt_client_) {
    LOG(INFO) << "Disconnecting GATT client for peripheral "
                      << peripheral_id_;
    gatt_client_->Disconnect();
    gatt_client_.reset();
  }
  
  if (gatt_server_) {
    LOG(INFO) << "Stopping GATT server for peripheral "
                      << peripheral_id_;
    // Server cleanup is handled by the server itself
    gatt_server_.reset();
  }
  
  return {Exception::kSuccess};
}

bool BleV2Socket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

// BleInputStream implementation
ExceptionOr<ByteArray> BleV2Socket::BleInputStream::Read(std::int64_t size) {
  absl::MutexLock lock(&mutex_);
  
  while (buffer_.Empty() && !closed_) {
    cond_.Wait(&mutex_);
  }

  if (closed_ && buffer_.Empty()) {
    return ExceptionOr<ByteArray>(Exception::kIo);
  }

  if (size < 0 || static_cast<size_t>(size) >= buffer_.size()) {
    ByteArray result = buffer_;
    buffer_ = ByteArray();
    return ExceptionOr<ByteArray>(result);
  }

  ByteArray result(buffer_.data(), size);
  buffer_ = ByteArray(buffer_.data() + size, buffer_.size() - size);
  return ExceptionOr<ByteArray>(result);
}

Exception BleV2Socket::BleInputStream::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }
  closed_ = true;
  cond_.SignalAll();
  return {Exception::kSuccess};
}

void BleV2Socket::BleInputStream::ReceiveData(const ByteArray& data) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return;
  }
  
  if (buffer_.Empty()) {
    buffer_ = data;
  } else {
    ByteArray combined(buffer_.size() + data.size());
    std::memcpy(combined.data(), buffer_.data(), buffer_.size());
    std::memcpy(combined.data() + buffer_.size(), data.data(), data.size());
    buffer_ = std::move(combined);
  }
  cond_.SignalAll();
}

void BleV2Socket::BleInputStream::NotifyClose() {
  absl::MutexLock lock(&mutex_);
  closed_ = true;
  cond_.SignalAll();
}

// BleOutputStream implementation
Exception BleV2Socket::BleOutputStream::Write(const ByteArray& data) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kIo};
  }

  if (!write_callback_) {
    LOG(WARNING) << "BleOutputStream: No write callback set";
    return {Exception::kIo};
  }

  bool success = write_callback_(data);
  return {success ? Exception::kSuccess : Exception::kIo};
}

Exception BleV2Socket::BleOutputStream::Flush() {
  return {Exception::kSuccess};
}

Exception BleV2Socket::BleOutputStream::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }
  closed_ = true;
  write_callback_ = nullptr;
  return {Exception::kSuccess};
}

void BleV2Socket::BleOutputStream::SetWriteCallback(WriteCallback callback) {
  absl::MutexLock lock(&mutex_);
  write_callback_ = std::move(callback);
}

void BleV2Socket::SetGattServer(
    std::unique_ptr<api::ble_v2::GattServer> gatt_server,
    const api::ble_v2::GattCharacteristic& rx_char,
    const api::ble_v2::GattCharacteristic& tx_char) {
  absl::MutexLock lock(&mutex_);
  
  if (closed_) {
    LOG(WARNING) << "Cannot set GATT server on closed socket";
    return;
  }
  
  gatt_server_ = std::move(gatt_server);
  rx_char_ = rx_char;
  tx_char_ = tx_char;

  // Set up write callback to use GATT server notifications
  output_stream_.SetWriteCallback(
      [this](const ByteArray& data) -> bool {
        absl::MutexLock lock(&mutex_);
        if (!gatt_server_) {
          LOG(ERROR) << "GATT server not available for write";
          return false;
        }
        
        if (closed_) {
          LOG(WARNING) << "Socket is closed, cannot write";
          return false;
        }
        
        // Notify remote device via TX characteristic
        absl::Status status = gatt_server_->NotifyCharacteristicChanged(
            tx_char_, /*confirm=*/false, data);
        
        if (!status.ok()) {
          LOG(WARNING) << "Failed to notify TX characteristic: "
                               << status.message();
          return false;
        }
        return true;
      });

  LOG(INFO) << "BLE socket configured with GATT server, RX: "
                    << std::string(rx_char.uuid) 
                    << ", TX: " << std::string(tx_char.uuid);
}

void BleV2Socket::SetGattClient(
    std::unique_ptr<api::ble_v2::GattClient> gatt_client,
    const api::ble_v2::GattCharacteristic& rx_char,
    const api::ble_v2::GattCharacteristic& tx_char) {
  absl::MutexLock lock(&mutex_);
  
  if (closed_) {
    LOG(WARNING) << "Cannot set GATT client on closed socket";
    return;
  }
  
  gatt_client_ = std::move(gatt_client);
  rx_char_ = rx_char;
  tx_char_ = tx_char;

  // Set up write callback to use GATT client writes
  output_stream_.SetWriteCallback(
      [this](const ByteArray& data) -> bool {
        absl::MutexLock lock(&mutex_);
        if (!gatt_client_) {
          LOG(ERROR) << "GATT client not available for write";
          return false;
        }
        
        if (closed_) {
          LOG(WARNING) << "Socket is closed, cannot write";
          return false;
        }
        
        // Write to TX characteristic on remote device
        std::string data_str(data.data(), data.size());
        bool success = gatt_client_->WriteCharacteristic(
            tx_char_, data_str,
            api::ble_v2::GattClient::WriteType::kWithoutResponse);
        
        if (!success) {
          LOG(WARNING) << "Failed to write to TX characteristic";
          return false;
        }
        return true;
      });

  // Subscribe to RX characteristic to receive data
  bool subscribed = gatt_client_->SetCharacteristicSubscription(
      rx_char_, /*enable=*/true,
      [this](absl::string_view value) {
        if (!IsClosed()) {
          ByteArray data(value.data(), value.size());
          input_stream_.ReceiveData(data);
        }
      });

  if (!subscribed) {
    LOG(ERROR) << "Failed to subscribe to RX characteristic";
  }

  LOG(INFO) << "BLE socket configured with GATT client, RX: "
                    << std::string(rx_char.uuid)
                    << ", TX: " << std::string(tx_char.uuid);
}

}  // namespace linux
}  // namespace nearby
