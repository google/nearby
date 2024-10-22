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

#include "internal/weave/sockets/client_socket.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "internal/platform/logging.h"
#include "internal/weave/base_socket.h"
#include "internal/weave/connection.h"
#include "internal/weave/packet.h"
#include "internal/weave/socket_callback.h"
#include "internal/weave/sockets/initial_data_provider.h"

namespace nearby {
namespace weave {
namespace {
constexpr int kConnectionConfirmPacketMinLength = 4;
}

ClientSocket::ClientSocket(const Connection& connection,
                           SocketCallback&& socket_callback)
    : BaseSocket(connection, std::move(socket_callback)) {
  initial_data_provider_ = std::make_unique<InitialDataProvider>();
}

ClientSocket::ClientSocket(
    const Connection& connection, SocketCallback&& socket_callback,
    std::unique_ptr<InitialDataProvider> initial_data_provider)
    : BaseSocket(connection, std::move(socket_callback)),
      initial_data_provider_(std::move(initial_data_provider)) {}

ClientSocket::~ClientSocket() {
  ShutDown();
  executor_.Shutdown();
  NEARBY_LOGS(INFO) << "ClientSocket gone.";
}

void ClientSocket::Connect() {
  if (state_ != State::kStateDisconnected) {
    return;
  }

  state_ = State::kStateClientConnectionRequest;
  WriteControlPacket(
      Packet::CreateConnectionRequestPacket(kProtocolVersion, kProtocolVersion,
                                            GetConnection().GetMaxPacketSize(),
                                            initial_data_provider_->Provide())
          .value());

  // We are now waiting for server confirmation, which we should receive in
  // OnReceiveControlPacket().
  state_ = State::kStateServerConfirm;
}

void ClientSocket::DisconnectQuietly() {
  BaseSocket::DisconnectQuietly();
  state_ = State::kStateDisconnected;
}

void ClientSocket::OnReceiveControlPacket(Packet packet) {
  if (packet.GetControlCommandNumber() ==
      Packet::ControlPacketType::kControlError) {
    DisconnectQuietly();
    return;
  }

  if (state_ != State::kStateServerConfirm) {
    GetSocketCallback().on_error_cb(absl::InternalError(
        absl::StrCat("unexpected control packet: ", packet.ToString())));
    return;
  }
  if (packet.GetControlCommandNumber() !=
      Packet::ControlPacketType::kControlConnectionConfirm) {
    DisconnectInternal(absl::InternalError(
        absl::StrCat("expected connection confirm packet but received ",
                     packet.ToString())));
    return;
  }
  if (packet.GetPayload().size() < kConnectionConfirmPacketMinLength) {
    DisconnectInternal(
        absl::InvalidArgumentError("packet of insufficient length received."));
    return;
  }
  int protocol_version =
      ((packet.GetPayload().data()[0] << 8) | packet.GetPayload().data()[1]) &
      (int16_t)0xFFFF;
  if (protocol_version != kProtocolVersion) {
    DisconnectInternal(absl::InternalError(
        absl::StrCat("unexpected protocol version ", protocol_version)));
    return;
  }
  int max_packet_size =
      ((packet.GetPayload().data()[2] << 8) | packet.GetPayload().data()[3]) &
      (int16_t)0xFFFF;
  if (max_packet_size > GetConnection().GetMaxPacketSize()) {
    DisconnectInternal(absl::InternalError(
        absl::StrCat("server confirmed max packet size ", max_packet_size,
                     " higher than connection's max packet size ",
                     GetConnection().GetMaxPacketSize())));
    return;
  }
  state_ = State::kStateHandshakeCompleted;
  OnConnected(max_packet_size);

  if (packet.GetPayload().size() > kConnectionConfirmPacketMinLength) {
    std::string remaining_data =
        packet.GetPayload().substr(kConnectionConfirmPacketMinLength);
    GetSocketCallback().on_receive_cb(remaining_data);
  }
}

}  // namespace weave
}  // namespace nearby
