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

#include "internal/socket_abstraction/v1/client_socket.h"

#include <string>
#include <utility>

#include "internal/platform/count_down_latch.h"
#include "internal/socket_abstraction/base_socket.h"
#include "internal/socket_abstraction/socket.h"
#include "internal/socket_abstraction/v1/initial_data_provider.h"

namespace nearby {
namespace socket_abstraction {

ClientSocket::ClientSocket(Connection* connection,
                           SocketCallback socket_callback)
    : BaseSocket(connection, std::move(socket_callback)) {
  initial_data_provider_ = new EmptyDataProvider();
  provider_needs_delete_ = true;
}

ClientSocket::ClientSocket(Connection* connection,
                           SocketCallback socket_callback,
                           InitialDataProvider& initial_data_provider)
    : BaseSocket(connection, std::move(socket_callback)) {
  initial_data_provider_ = &initial_data_provider;
}

ClientSocket::~ClientSocket() {
  CountDownLatch latch(1);
  executor_.Execute("Cleaner", [&latch]() { latch.CountDown(); });
  latch.Await();
  executor_.Shutdown();
  if (provider_needs_delete_) {
    delete initial_data_provider_;
  }
}

void ClientSocket::Connect() {
  if (state_ != State::kStateDisconnected) {
    return;
  }

  state_ = State::kStateClientConnectionRequest;
  auto result = WriteControlPacket(
      Packet::CreateConnectionRequestPacket(kProtocolVersion, kProtocolVersion,
                                            connection_->GetMaxPacketSize(),
                                            initial_data_provider_->Provide())
          .value());
  executor_.Execute(
      "Connect delayed", [this, result = std::move(result)]() mutable {
        result.wait();
        auto status = result.get();
        if (!status.ok()) {
          DisconnectInternal(
              absl::InternalError("transmit of connection request failed"));
        }

        if (state_ == State::kStateHandshakeCompleted) {
          return;
        }

        if (state_ != State::kStateClientConnectionRequest) {
          socket_callback_.on_error_cb(
              absl::InternalError("unexpected transmit of connection request"));
        }
        state_ = State::kStateServerConfirm;
      });
}

bool ClientSocket::IsConnectingOrConnected() {
  return state_ != State::kStateDisconnected;
}

void ClientSocket::DisconnectQuietly() {
  BaseSocket::DisconnectQuietly();
  state_ = State::kStateDisconnected;
}

void ClientSocket::OnReceiveControlPacket(Packet packet) {
  if (packet.GetControlCommandNumber() == Packet::kControlError) {
    DisconnectQuietly();
    return;
  }

  if (state_ != State::kStateServerConfirm &&
      state_ != State::kStateClientConnectionRequest) {
    socket_callback_.on_error_cb(absl::InternalError(
        absl::StrFormat("unexpected control packet: %s", packet.ToString())));
  }
  if (packet.GetControlCommandNumber() != Packet::kControlConnectionConfirm) {
    DisconnectInternal(absl::InternalError(
        absl::StrFormat("expected connection confirm packet but received %s",
                        packet.ToString())));
    return;
  }
  int protocol_version =
      ((packet.GetPayload().data()[1] << 8) | packet.GetPayload().data()[0]) &
      (int16_t)0xFFFF;
  if (protocol_version != kProtocolVersion) {
    DisconnectInternal(absl::InternalError(
        absl::StrFormat("unexpected protocol version %d", protocol_version)));
    return;
  }
  int max_packet_size =
      ((packet.GetPayload().data()[3] << 8) | packet.GetPayload().data()[2]) &
      (int16_t)0xFFFF;
  if (max_packet_size > connection_->GetMaxPacketSize()) {
    DisconnectInternal(absl::InternalError(
        absl::StrFormat("server confirmed max packet size %d higher than "
                        "connection's max packet size %d",
                        max_packet_size, connection_->GetMaxPacketSize())));
    return;
  }
  state_ = State::kStateHandshakeCompleted;
  OnConnected(max_packet_size);

  if (packet.GetPayload().size() > 4) {
    ByteArray remaining_data =
        ByteArray(std::string(packet.GetPayload().AsStringView().substr(4)));
    socket_callback_.on_receive_cb(remaining_data);
  }
}

}  // namespace socket_abstraction
}  // namespace nearby
