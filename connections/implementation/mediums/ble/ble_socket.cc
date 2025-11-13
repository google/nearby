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

#include "connections/implementation/mediums/ble/ble_socket.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {

using ::location::nearby::proto::connections::Medium;

ExceptionOr<ByteArray> BleInputStream::Read(std::int64_t size) {
  return source_.Read(size);
}

Exception BleInputStream::Close() { return source_.Close(); }

Exception BleOutputStream::Write(const ByteArray& data) {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleOutputStream::Flush() { return source_.Flush(); }

Exception BleOutputStream::Close() { return source_.Close(); }

BleSocket::BleSocket(const ByteArray& service_id_hash,
                     std::unique_ptr<BleInputStream> ble_input_stream,
                     std::unique_ptr<BleOutputStream> ble_output_stream,
                     nearby::BleSocket ble_socket)
    : service_id_hash_(service_id_hash),
      ble_input_stream_(std::move(ble_input_stream)),
      ble_output_stream_(std::move(ble_output_stream)),
      ble_socket_(std::move(ble_socket)) {}

BleSocket::BleSocket(const ByteArray& service_id_hash,
                     std::unique_ptr<BleInputStream> ble_input_stream,
                     std::unique_ptr<BleOutputStream> ble_output_stream,
                     nearby::BleL2capSocket l2cap_socket)
    : service_id_hash_(service_id_hash),
      ble_input_stream_(std::move(ble_input_stream)),
      ble_output_stream_(std::move(ble_output_stream)),
      l2cap_socket_(std::move(l2cap_socket)) {}

BleSocket::~BleSocket() { Close(); }

InputStream& BleSocket::GetInputStream() {
  MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    LOG(FATAL) << "GetInputStream() called on a closed or invalid BleSocket.";
  }

  return *ble_input_stream_;
}

OutputStream& BleSocket::GetOutputStream() {
  MutexLock lock(&mutex_);
  if (!ble_output_stream_) {
    LOG(FATAL) << "GetOutputStream() called on a closed or invalid BleSocket.";
  }

  return *ble_output_stream_;
}

Exception BleSocket::Close() {
  MutexLock lock(&mutex_);
  return CloseLocked();
}

Exception BleSocket::CloseLocked() {
  if (!ble_input_stream_ && !ble_output_stream_) {
    return {Exception::kSuccess};
  }
  if (ble_input_stream_) {
    ble_input_stream_->Close();
    ble_input_stream_.reset();
  }
  if (ble_output_stream_) {
    ble_output_stream_->Close();
    ble_output_stream_.reset();
  }
  Medium medium = GetMediumLocked();
  switch (medium) {
    case Medium::BLE:
      return ble_socket_.Close();
    case Medium::BLE_L2CAP:
      return l2cap_socket_.Close();
    default:
      LOG(FATAL) << "Socket close on unknown medium.";
      break;
  }
  return {Exception::kIo};
}

nearby::BlePeripheral& BleSocket::GetRemotePeripheral() {
  MutexLock lock(&mutex_);
  Medium medium = GetMediumLocked();
  switch (medium) {
    case Medium::BLE:
      return ble_socket_.GetRemotePeripheral();
    case Medium::BLE_L2CAP:
      return l2cap_socket_.GetRemotePeripheral();
    default:
      LOG(FATAL) << "BleSocket has no valid underlying socket.";
      break;
  }
}

bool BleSocket::IsValid() const {
  MutexLock lock(&mutex_);
  Medium medium = GetMediumLocked();
  switch (medium) {
    case Medium::BLE:
      return ble_socket_.IsValid();
    case Medium::BLE_L2CAP:
      return l2cap_socket_.IsValid();
    default:
      LOG(FATAL) << "BleSocket has no valid underlying socket.";
      break;
  }
}

Medium BleSocket::GetMedium() const {
  MutexLock lock(&mutex_);
  return GetMediumLocked();
}

Medium BleSocket::GetMediumLocked() const {
  if (ble_socket_.IsValid()) {
    return Medium::BLE;
  }
  if (l2cap_socket_.IsValid()) {
    return Medium::BLE_L2CAP;
  }
  return Medium::UNKNOWN_MEDIUM;
}

ExceptionOr<ByteArray> BleSocket::DispatchPacket() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

ExceptionOr<std::int32_t> BleSocket::ReadPayloadLength() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::WritePayloadLength(int payload_length) {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::SendIntroduction() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::SendDisconnection() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::SendPacketAcknowledgement(int received_size) {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::ProcessIncomingL2capPacketValidation() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

Exception BleSocket::ProcessOutgoingL2capPacketValidation() {
  // TODO(b/419654808): Implement this method.
  return {Exception::kFailed};
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
