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

#include "internal/weave/sockets/server_socket.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/logging.h"
#include "internal/weave/base_socket.h"
#include "internal/weave/connection.h"
#include "internal/weave/packet.h"
#include "internal/weave/socket_callback.h"

namespace nearby {
namespace weave {
namespace {
constexpr int kMinimumConnectionRequestLength = 6;
constexpr int kProtocolVersion = 1;

// Constants for selecting min/max protocol version and max packet size.
constexpr int kMinProtocolMsbIndex = 0;
constexpr int kMinProtocolLsbIndex = 1;
constexpr int kMaxProtocolMsbIndex = 2;
constexpr int kMaxProtocolLsbIndex = 3;
constexpr int kMaxPacketSizeMsbIndex = 4;
constexpr int kMaxPacketSizeLsbIndex = 5;

int16_t ExtractMinProtocolVersionFromConnRequest(
    absl::string_view packet_bytes) {
  return ((packet_bytes.data()[kMinProtocolMsbIndex] << 8) |
          packet_bytes.data()[kMinProtocolLsbIndex]) &
         0xFFFF;
}

int16_t ExtractMaxProtocolVersionFromConnRequest(
    absl::string_view packet_bytes) {
  return ((packet_bytes.data()[kMaxProtocolMsbIndex] << 8) |
          packet_bytes.data()[kMaxProtocolLsbIndex]) &
         0xFFFF;
}

uint16_t ExtractMaxPacketSizeFromConnRequest(absl::string_view packet_bytes) {
  return ((packet_bytes.data()[kMaxPacketSizeMsbIndex] << 8) |
          packet_bytes.data()[kMaxPacketSizeLsbIndex]) &
         0xFFFF;
}
}  // namespace

ServerSocket::ServerSocket(const Connection& connection,
                           SocketCallback&& socket_callback)
    : BaseSocket(connection, std::move(socket_callback)) {}

ServerSocket::~ServerSocket() {
  NEARBY_LOGS(INFO) << "ServerSocket dtor";
  ShutDown();
}

void ServerSocket::DisconnectQuietly() {
  BaseSocket::DisconnectQuietly();
  state_ = State::kDisconnected;
}

void ServerSocket::OnReceiveControlPacket(Packet packet) {
  if (packet.GetControlCommandNumber() ==
      Packet::ControlPacketType::kControlError) {
    NEARBY_LOGS(WARNING) << "Received error control packet, disconnecting.";
    DisconnectQuietly();
    return;
  }
  // Control packets besides errors are not supposed to be sent or received
  // after the initial handshake.
  if (state_ != State::kClientConnectionRequest) {
    NEARBY_LOGS(ERROR) << "Not in 'Connection Request' state, but "
                          "incorrectly received control packet of type "
                       << Packet::ControlPacketTypeToString(
                              packet.GetControlCommandNumber());
    GetSocketCallback().on_error_cb(
        absl::InvalidArgumentError("Unexpected control packet"));
    return;
  }
  if (packet.GetControlCommandNumber() !=
      Packet::ControlPacketType::kControlConnectionRequest) {
    NEARBY_LOGS(ERROR) << "Expected connection request control packet, "
                          "received control packet of type "
                       << Packet::ControlPacketTypeToString(
                              packet.GetControlCommandNumber());
    DisconnectInternal(
        absl::InvalidArgumentError("Unexpected control packet type."));
    return;
  }
  std::string packet_payload = packet.GetPayload();
  if (packet_payload.size() < kMinimumConnectionRequestLength) {
    GetSocketCallback().on_error_cb(absl::InvalidArgumentError(
        "Insufficient length connection request packet received."));
    return;
  }
  int min_protocol_version =
      ExtractMinProtocolVersionFromConnRequest(packet_payload);
  int max_protocol_version =
      ExtractMaxProtocolVersionFromConnRequest(packet_payload);
  if (min_protocol_version > kProtocolVersion ||
      max_protocol_version < kProtocolVersion) {
    NEARBY_LOGS(ERROR) << "Received unexpected min/max protocol versions: "
                       << min_protocol_version << " and "
                       << max_protocol_version;
    DisconnectInternal(
        absl::InvalidArgumentError("unexpected protocol versions"));
    return;
  }
  int client_max_packet_size =
      ExtractMaxPacketSizeFromConnRequest(packet_payload);
  if (client_max_packet_size == 0) {
    max_packet_size_ = GetConnection().GetMaxPacketSize();
  } else {
    max_packet_size_ =
        std::min(GetConnection().GetMaxPacketSize(), client_max_packet_size);
  }
  if (packet_payload.size() > kMinimumConnectionRequestLength) {
    std::string remaining_data =
        packet_payload.substr(kMinimumConnectionRequestLength);
    GetSocketCallback().on_receive_cb(remaining_data);
  }
  WriteConnectionConfirm();
}

void ServerSocket::WriteConnectionConfirm() {
  state_ = State::kServerConfirm;
  absl::StatusOr<Packet> packet = Packet::CreateConnectionConfirmPacket(
      kProtocolVersion, max_packet_size_, "");
  if (!packet.ok()) {
    NEARBY_LOGS(ERROR) << "Failed to create connection confirm packet: "
                       << packet.status();
    DisconnectInternal(packet.status());
    return;
  }
  WriteControlPacket(std::move(*packet));
  state_ = State::kHandshakeCompleted;
  OnConnected(max_packet_size_);
}

}  // namespace weave
}  // namespace nearby
