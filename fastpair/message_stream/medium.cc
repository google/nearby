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

#include "fastpair/message_stream/medium.h"

#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/constant.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/future.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr int kHeaderSize = 4;
}

absl::Status Medium::OpenRfcomm() {
  if (!bt_classic_medium_.has_value()) {
    return absl::FailedPreconditionError("BT classic unsupported");
  }
  if (!device_.GetPublicAddress().has_value()) {
    return absl::FailedPreconditionError(
        "Connect open RFCOMM without public BT address");
  }
  BluetoothClassicMedium* classic_medium = bt_classic_medium_.value();
  executor_.Execute("open-rfcomm", [this, classic_medium]() {
    if (cancellation_flag_.Cancelled()) return;
    BluetoothDevice device =
        classic_medium->GetRemoteDevice(device_.GetPublicAddress().value());
    if (!device.IsValid()) {
      observer_.OnConnectionResult(absl::UnavailableError(
          absl::StrFormat("Remote BT device %s not found",
                          device_.GetPublicAddress().value())));
      return;
    }
    SetSocket(classic_medium->ConnectToService(device, kRfcommUuid,
                                               &cancellation_flag_));
    BluetoothSocket socket = GetSocket();
    absl::Status status = socket.IsValid()
                              ? absl::OkStatus()
                              : absl::UnavailableError(absl::StrFormat(
                                    "Failed to open RFCOMM with %s",
                                    device_.GetPublicAddress().value()));
    observer_.OnConnectionResult(status);
    if (status.ok()) {
      RunLoop(std::move(socket));
    }
  });
  return absl::OkStatus();
}

// Opens L2CAP connection with the remote party.
// Returns an error if a connection attempt could not be made.
// Otherwise, `OnConnected()` will be called if the connection was successful,
// or `OnDisconnected()` if connection failed.
absl::Status Medium::OpenL2cap(absl::string_view ble_address) {
  return absl::UnimplementedError("L2CAP unimplemented");
}

absl::Status Medium::Disconnect() {
  NEARBY_LOGS(INFO) << "Disconnect";
  cancellation_flag_.Cancel();
  CloseSocket();
  return absl::OkStatus();
}

// Returns OK if the message was queued for delivery. It does not mean the
// message was delivered to the remote party.
absl::Status Medium::Send(Message message, bool compute_and_append_mac) {
  BluetoothSocket socket = GetSocket();
  if (cancellation_flag_.Cancelled() || !socket.IsValid()) {
    return absl::FailedPreconditionError("Not connected");
  }
  ByteArray byte_array = Serialize(std::move(message), compute_and_append_mac);
  if (socket.GetOutputStream().Write(byte_array).Ok()) {
    return absl::OkStatus();
  } else {
    cancellation_flag_.Cancel();
    return absl::DataLossError("Failed to send data to remote");
  }
}

void Medium::RunLoop(BluetoothSocket socket) {
  NEARBY_LOGS(INFO) << "Run loop";
  InputStream& input = socket.GetInputStream();
  while (!cancellation_flag_.Cancelled()) {
    ExceptionOr<ByteArray> header = input.ReadExactly(kHeaderSize);
    if (!header.ok() || header.result().size() != kHeaderSize) {
      break;
    }
    absl::string_view data = header.result().AsStringView();
    MessageGroup group = static_cast<MessageGroup>(data[0]);
    MessageCode code = static_cast<MessageCode>(data[1]);
    int length = static_cast<unsigned int>(data[2]) * 256 +
                 static_cast<unsigned int>(data[3]);
    ExceptionOr<ByteArray> payload;
    if (length > 0) {
      payload = input.ReadExactly(length);
    } else {
      payload = ExceptionOr<ByteArray>(ByteArray(""));
    }
    if (!payload.ok() || payload.result().size() != length) {
      break;
    }
    observer_.OnReceived(Message{.message_group = group,
                                 .message_code = code,
                                 .payload = std::string(payload.result())});
  }
  socket.Close();
  if (!cancellation_flag_.Cancelled()) {
    observer_.OnDisconnected(absl::DataLossError("Failed to read from remote"));
  }
  NEARBY_LOGS(INFO) << "Run loop done";
}

ByteArray Medium::Serialize(Message message, bool compute_and_append_mac) {
  DCHECK_EQ(compute_and_append_mac, false);
  uint16_t payload_size = message.payload.size();
  int message_size = kHeaderSize + payload_size;
  ByteArray byte_array = ByteArray(message_size);
  uint8_t* data = reinterpret_cast<uint8_t*>(byte_array.data());
  data[0] = static_cast<uint8_t>(message.message_group);
  data[1] = static_cast<uint8_t>(message.message_code);
  data[2] = payload_size >> 8;
  data[3] = payload_size & 0xFF;
  memcpy(&data[kHeaderSize], message.payload.data(), payload_size);
  return byte_array;
}

void Medium::SetSocket(BluetoothSocket socket) {
  if (!socket.IsValid()) return;
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "SetSocket 1";
  if (cancellation_flag_.Cancelled()) {
    NEARBY_LOGS(INFO) << "Medium already closed. Closing socket";
    socket.Close();
  } else {
    NEARBY_LOGS(INFO) << "SetSocket 2";
    socket_ = std::move(socket);
  }
}

BluetoothSocket Medium::GetSocket() {
  MutexLock lock(&mutex_);
  // Returns a stable copy of the socket. Both `socket_` and the returned copy
  // refer to the same platform socket.
  return socket_;
}

void Medium::CloseSocket() {
  MutexLock lock(&mutex_);
  if (socket_.IsValid()) {
    socket_.Close();
    socket_ = BluetoothSocket();
  }
}

}  // namespace fastpair
}  // namespace nearby
