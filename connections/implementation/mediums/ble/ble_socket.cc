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
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble/ble_packet.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/byte_utils.h"
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
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kRefactorBleL2cap)) {
    if (!payload_length_) {
      return {Exception::kFailed};
    }
    // Prepend the packet length to the data.
    std::string packet_str =
        absl::StrCat(std::string(byte_utils::IntToBytes(payload_length_)),
                     std::string(data));
    payload_length_ = 0;

    // Prepend the service id hash to the data with the payload length.
    absl::StatusOr<BlePacket> ble_packet_status_or =
        BlePacket::CreateDataPacket(service_id_hash_,
                                    ByteArray(std::move(packet_str)));
    if (!ble_packet_status_or.ok()) {
      return {Exception::kFailed};
    }
    return source_.Write(ByteArray(ble_packet_status_or.value()));
  } else {
    return source_.Write(data);
  }
}

Exception BleOutputStream::Flush() { return source_.Flush(); }

Exception BleOutputStream::Close() { return source_.Close(); }

Exception BleOutputStream::WritePayloadLength(int payload_length) {
  if (payload_length_ != 0) {
    return {Exception::kFailed};
  }
  // Store the payload length to be prepended to the data later.
  payload_length_ = payload_length;
  return {Exception::kSuccess};
}

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
  MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    return Exception::kFailed;
  }

  ExceptionOr<ByteArray> read_bytes =
      ble_input_stream_->Read(BlePacket::kServiceIdHashLength);
  while (read_bytes.ok()) {
    ByteArray read_bytes_result = read_bytes.result();
    if (BlePacket::IsControlPacketBytes(read_bytes_result)) {
      ExceptionOr<ByteArray> handle_result = ProcessBleControlPacketLocked();
      if (!handle_result.ok()) {
        return handle_result;
      }
      read_bytes = ble_input_stream_->Read(BlePacket::kServiceIdHashLength);
    } else {
      if (read_bytes_result != service_id_hash_) {
        LOG(WARNING)
            << "Received data packet with incorrect service ID hash. Expected: "
            << absl::BytesToHexString(service_id_hash_.string_data())
            << ", Received: "
            << absl::BytesToHexString(read_bytes_result.string_data());
        return Exception::kFailed;
      }
      break;
    }
  }
  return read_bytes;
}

ExceptionOr<std::int32_t> BleSocket::ReadPayloadLength() {
  MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    return {Exception::kIo};
  }

  ExceptionOr<ByteArray> read_bytes =
      ble_input_stream_->Read(sizeof(std::int32_t));
  if (!read_bytes.ok()) {
    return read_bytes.exception();
  }

  int payload_length = byte_utils::BytesToInt(std::move(read_bytes.result()));
  return ExceptionOr<std::int32_t>(payload_length);
}

Exception BleSocket::WritePayloadLength(int payload_length) {
  MutexLock lock(&mutex_);
  if (!ble_output_stream_) {
    return {Exception::kIo};
  }
  return ble_output_stream_->WritePayloadLength(payload_length);
}

ExceptionOr<ByteArray> BleSocket::ProcessBleControlPacketLocked() {
  // Read the first 4 bytes (packet block 1).
  ExceptionOr<ByteArray> read_bytes = ble_input_stream_->Read(4);
  if (!read_bytes.ok()) {
    return read_bytes;
  }
  if (read_bytes.result().size() != 4) {
    return Exception::kFailed;
  }
  ByteArray packet_block_1 = read_bytes.result();

  // Read the length from the 3rd byte of the packet block (0-indexed).
  int packet_block_2_size = packet_block_1.data()[3];
  // Read the left bytes for the packet block 2).
  read_bytes = ble_input_stream_->Read(packet_block_2_size);
  if (!read_bytes.ok()) {
    return read_bytes;
  }
  if (read_bytes.result().size() != packet_block_2_size) {
    return Exception::kFailed;
  }
  ByteArray packet_block_2 = read_bytes.result();

  // Concatenate the two packet blocks.
  std::string str1(packet_block_1);
  std::string str2(packet_block_2);
  std::string result_str = absl::StrCat(str1, str2);
  ByteArray packet_block = ByteArray(result_str);

  // Create the BlePacket from the concatenated packet block.
  absl::StatusOr<BlePacket> ble_packet_status_or =
      BlePacket::CreateControlPacket(packet_block);
  if (!ble_packet_status_or.ok()) {
    return Exception::kFailed;
  }
  BlePacket ble_packet = ble_packet_status_or.value();
  ble_packet.ParseControlPacketData(packet_block.AsStringView());
  if (!ble_packet.IsValid()) {
    return Exception::kFailed;
  }
  if (service_id_hash_ != ble_packet.GetServiceIdHash()) {
    return Exception::kFailed;
  }
  LOG(INFO) << "Received BLE Socket Control frame: "
            << BlePacket::SocketControlFrameTypeToString(
                   ble_packet.GetControlFrameType());
  return Exception::kSuccess;
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
