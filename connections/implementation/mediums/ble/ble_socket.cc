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
#include "absl/strings/string_view.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble/ble_l2cap_packet.h"
#include "connections/implementation/mediums/ble/ble_packet.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/byte_utils.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace connections {
namespace mediums {

using ::location::nearby::proto::connections::Medium;

ExceptionOr<ByteArray> BleInputStream::Read(std::int64_t size) {
  return source_.Read(size);
}

Exception BleInputStream::Close() { return source_.Close(); }

Exception BleOutputStream::Write(absl::string_view data) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kRefactorBleL2cap)) {
    if (!payload_length_) {
      return {Exception::kFailed};
    }
    // Prepend the packet length to the data.
    std::string packet_str =
        absl::StrCat(std::string(byte_utils::IntToBytes(payload_length_)),
                     data);
    payload_length_ = 0;

    // Prepend the service id hash to the data with the payload length.
    absl::StatusOr<BlePacket> ble_packet_status_or =
        BlePacket::CreateDataPacket(service_id_hash_,
                                    ByteArray(std::move(packet_str)));
    if (!ble_packet_status_or.ok()) {
      return {Exception::kFailed};
    }
    return source_.Write(
        ByteArray(ble_packet_status_or.value()).AsStringView());
  } else {
    return source_.Write(data);
  }
}

Exception BleOutputStream::Flush() { return source_.Flush(); }

Exception BleOutputStream::Close() { return source_.Close(); }

Exception BleOutputStream::WriteControlPacket(const ByteArray& data) {
  return source_.Write(data.AsStringView());
}

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
  serial_executor_.Shutdown();
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
  int payload_length = 0;
  {
    MutexLock lock(&mutex_);
    if (!ble_input_stream_) {
      return {Exception::kIo};
    }

    ExceptionOr<ByteArray> read_bytes =
        ble_input_stream_->Read(sizeof(std::int32_t));
    if (!read_bytes.ok()) {
      return read_bytes.exception();
    }

    payload_length = byte_utils::BytesToInt(std::move(read_bytes.result()));
  }
  Exception send_ack_result = SendPacketAcknowledgement(payload_length);
  if (!send_ack_result.Ok()) {
    LOG(WARNING) << "Failed to send packet acknowledgement.";
  }
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
  Exception result = {Exception::kFailed};
  CountDownLatch latch(1);
  RunOnSocketThread([this, &latch, &result]() {
    MutexLock lock(&mutex_);
    if (!ble_output_stream_) {
      latch.CountDown();
      return;
    }

    absl::StatusOr<BlePacket> ble_packet_status_or =
        BlePacket::CreateControlIntroductionPacket(service_id_hash_);
    if (!ble_packet_status_or.ok()) {
      LOG(WARNING) << "Failed to create BLE introduction packet: "
                   << ble_packet_status_or.status();
      latch.CountDown();
      return;
    }
    result = ble_output_stream_->WriteControlPacket(
        ByteArray(ble_packet_status_or.value()));
    if (!result.Ok()) {
      LOG(WARNING) << "Failed to write BLE introduction packet: "
                   << result.value;
    }
    latch.CountDown();
  });
  latch.Await();
  return result;
}

Exception BleSocket::SendDisconnection() {
  Exception result = {Exception::kFailed};
  CountDownLatch latch(1);
  RunOnSocketThread([this, &latch, &result]() {
    MutexLock lock(&mutex_);
    if (!ble_output_stream_) {
      latch.CountDown();
      return;
    }

    absl::StatusOr<BlePacket> ble_packet_status_or =
        BlePacket::CreateControlDisconnectionPacket(service_id_hash_);
    if (!ble_packet_status_or.ok()) {
      LOG(WARNING) << "Failed to create BLE control disconnection packet: "
                   << ble_packet_status_or.status();
      latch.CountDown();
      return;
    }
    result = ble_output_stream_->WriteControlPacket(
        ByteArray(ble_packet_status_or.value()));
    if (!result.Ok()) {
      LOG(WARNING) << "Failed to write BLE control disconnection packet: "
                   << result.value;
    }
    latch.CountDown();
  });
  latch.Await();
  return result;
}

Exception BleSocket::SendPacketAcknowledgement(int received_size) {
  Exception result = {Exception::kFailed};
  CountDownLatch latch(1);
  RunOnSocketThread([this, &latch, &result, &received_size]() {
    MutexLock lock(&mutex_);
    if (!ble_output_stream_) {
      latch.CountDown();
      return;
    }

    absl::StatusOr<BlePacket> ble_packet_status_or =
        BlePacket::CreateControlPacketAcknowledgementPacket(service_id_hash_,
                                                            received_size);
    if (!ble_packet_status_or.ok()) {
      LOG(WARNING)
          << "Failed to create BLE control packet acknowledgement packet: "
          << ble_packet_status_or.status();
      latch.CountDown();
      return;
    }
    result = ble_output_stream_->WriteControlPacket(
        ByteArray(ble_packet_status_or.value()));
    if (!result.Ok()) {
      LOG(WARNING)
          << "Failed to write BLE control packet acknowledgement packet: "
          << result.value;
    }
    latch.CountDown();
  });
  latch.Await();
  return result;
}

Exception BleSocket::ProcessIncomingL2capPacketValidation() {
  MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    return {Exception::kFailed};
  }

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet_status_or =
      BleL2capPacket::CreateFromStream(*ble_input_stream_);
  if (!ble_l2cap_packet_status_or.ok()) {
    LOG(WARNING) << "Failed to create BleL2capPacket: "
                 << ble_l2cap_packet_status_or.status();
    return {Exception::kFailed};
  }

  // Make sure the packet is a data connection request.
  BleL2capPacket ble_l2cap_packet = ble_l2cap_packet_status_or.value();
  if (!ble_l2cap_packet.IsDataConnectionRequest()) {
    LOG(WARNING)
        << "Received an L2CAP packet that is not a data connection request.";
    return {Exception::kFailed};
  }

  // Send out the Command::kResponseDataConnectionReady packet.
  Exception result =
      SendL2capPacketLocked(BleL2capPacket::ByteArrayForDataConnectionReady());
  if (!result.Ok()) {
    LOG(WARNING)
        << "Failed to send L2CAP data connection ready response packet: "
        << result.value;
    return result;
  }

  return result;
}

Exception BleSocket::ProcessOutgoingL2capPacketValidation() {
  MutexLock lock(&mutex_);
  if (!ble_input_stream_) {
    return {Exception::kFailed};
  }

  // Send out the Command::kRequestDataConnection packet.
  Exception result = SendL2capPacketLocked(
      BleL2capPacket::ByteArrayForRequestDataConnection());
  if (!result.Ok()) {
    LOG(WARNING) << "Failed to send L2CAP request data connection packet: "
                 << result.value;
    return result;
  }

  // Wait here for the Command::kResponseDataConnectionReady packet.
  absl::StatusOr<BleL2capPacket> ble_l2cap_packet_status_or =
      BleL2capPacket::CreateFromStream(*ble_input_stream_);
  if (!ble_l2cap_packet_status_or.ok()) {
    LOG(WARNING) << "Failed to create BleL2capPacket: "
                 << ble_l2cap_packet_status_or.status();
    return {Exception::kFailed};
  }
  BleL2capPacket ble_l2cap_packet = ble_l2cap_packet_status_or.value();
  if (!ble_l2cap_packet.IsDataConnectionReadyResponse()) {
    LOG(WARNING) << "Unexpected L2CAP packet received.";
    return {Exception::kFailed};
  }
  return {Exception::kSuccess};
}

Exception BleSocket::SendL2capPacketLocked(const ByteArray& packet_byte) {
  if (!ble_output_stream_) {
    return {Exception::kFailed};
  }

  Exception result = {Exception::kFailed};
  CountDownLatch latch(1);
  RunOnSocketThread([this, &packet_byte, &latch, &result]() {
    mutex_.AssertHeld();

    result = ble_output_stream_->WriteControlPacket(packet_byte);
    if (!result.Ok()) {
      LOG(WARNING) << "Failed to write L2CAP packet: " << result.value;
    }
    latch.CountDown();
  });
  latch.Await();
  return result;
}

void BleSocket::RunOnSocketThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
