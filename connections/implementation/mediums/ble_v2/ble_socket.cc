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

#include "connections/implementation/mediums/ble_v2/ble_socket.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
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
  return {Exception::kFailed};
}

Exception BleOutputStream::Flush() { return source_.Flush(); }

Exception BleOutputStream::Close() { return source_.Close(); }

BleSocket::BleSocket(const ByteArray& service_id_hash,
                     std::unique_ptr<BleInputStream> ble_input_stream,
                     std::unique_ptr<BleOutputStream> ble_output_stream,
                     BleV2Socket ble_socket)
    : service_id_hash_(service_id_hash),
      ble_input_stream_(std::move(ble_input_stream)),
      ble_output_stream_(std::move(ble_output_stream)),
      ble_socket_(std::move(ble_socket)) {}

BleSocket::BleSocket(const ByteArray& service_id_hash,
                     std::unique_ptr<BleInputStream> ble_input_stream,
                     std::unique_ptr<BleOutputStream> ble_output_stream,
                     BleL2capSocket l2cap_socket)
    : service_id_hash_(service_id_hash),
      ble_input_stream_(std::move(ble_input_stream)),
      ble_output_stream_(std::move(ble_output_stream)),
      l2cap_socket_(std::move(l2cap_socket)) {}

BleSocket::~BleSocket() { Close(); }

InputStream& BleSocket::GetInputStream() {
  absl::MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    LOG(FATAL) << "GetInputStream() called on a closed or invalid BleSocket.";
  }

  return *ble_input_stream_;
}

OutputStream& BleSocket::GetOutputStream() {
  absl::MutexLock lock(&mutex_);
  if (!ble_output_stream_) {
    LOG(FATAL) << "GetOutputStream() called on a closed or invalid BleSocket.";
  }

  return *ble_output_stream_;
}

Exception BleSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return CloseLocked();
}

Exception BleSocket::CloseLocked() {
  if (ble_input_stream_) {
    ble_input_stream_->Close();
  }
  if (ble_output_stream_) {
    ble_output_stream_->Close();
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

BleV2Peripheral& BleSocket::GetRemotePeripheral() {
  absl::MutexLock lock(&mutex_);
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
  absl::MutexLock lock(&mutex_);
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
  absl::MutexLock lock(&mutex_);
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
  return {Exception::kFailed};
}

ExceptionOr<std::int32_t> BleSocket::ReadPayloadLength() {
  return {Exception::kFailed};
}

Exception BleSocket::WritePayloadLength(int payload_length) {
  return {Exception::kFailed};
}

Exception BleSocket::SendIntroduction() {
  return {Exception::kFailed};
}

Exception BleSocket::SendDisconnection() {
return {Exception::kFailed};
}

Exception BleSocket::SendPacketAcknowledgement(int received_size) {
  return {Exception::kFailed};
}

Exception BleSocket::ProcessIncomingL2capPacketValidation() {
  return {Exception::kFailed};
}

Exception BleSocket::ProcessOutgoingL2capPacketValidation() {
  return {Exception::kFailed};
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
