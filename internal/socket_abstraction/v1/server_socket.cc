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

#include "internal/socket_abstraction/v1/server_socket.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "internal/platform/count_down_latch.h"
#include "internal/socket_abstraction/base_socket.h"
#include "internal/socket_abstraction/v1/initial_data_provider.h"

namespace nearby {
namespace socket_abstraction {

ServerSocket::ServerSocket(Connection* connection,
                           SocketCallback socketCallback)
    : BaseSocket(connection, std::move(socketCallback)) {
  initial_data_provider_ = new EmptyDataProvider();
  provider_needs_delete_ = true;
}

ServerSocket::~ServerSocket() {
  CountDownLatch latch(1);
  executor_.Execute("Cleaner", [&latch]() { latch.CountDown(); });
  latch.Await();
  executor_.Shutdown();
  if (provider_needs_delete_) {
    delete initial_data_provider_;
  }
}

ServerSocket::ServerSocket(Connection* connection,
                           SocketCallback socketCallback,
                           InitialDataProvider& dataProvider)
    : BaseSocket::BaseSocket(connection, std::move(socketCallback)) {
  initial_data_provider_ = &dataProvider;
}

void ServerSocket::DisconnectQuietly() {
  state_ = State::kStateClientConnectionRequest;
  BaseSocket::DisconnectQuietly();
}

void ServerSocket::OnReceiveControlPacket(Packet packet) {
  if (packet.GetControlCommandNumber() == Packet::kControlError) {
    DisconnectQuietly();
    return;
  }
  if (state_ != State::kStateClientConnectionRequest) {
    socket_callback_.on_error_cb(
        absl::InvalidArgumentError("unexpected control packet"));
    return;
  }
  if (packet.GetControlCommandNumber() != Packet::kControlConnectionRequest) {
    DisconnectInternal(absl::InvalidArgumentError("unexpected control packet"));
    return;
  }
  int min_protocol_version =
      ((packet.GetPayload().data()[1] << 8) | packet.GetPayload().data()[0]) &
      0xFFFF;
  int max_protocol_version =
      ((packet.GetPayload().data()[3] << 8) | packet.GetPayload().data()[2]) &
      0xFFFF;
  if (min_protocol_version > kProtocolVersion ||
      max_protocol_version < kProtocolVersion) {
    DisconnectInternal(
        absl::InvalidArgumentError("unexpected protocol versions"));
    return;
  }
  int client_max_packet_size =
      ((packet.GetPayload().data()[5] << 8) | packet.GetPayload().data()[4]) &
      0xFF;
  if (client_max_packet_size == 0) {
    max_packet_size_ = connection_->GetMaxPacketSize();
  } else {
    max_packet_size_ =
        std::min(connection_->GetMaxPacketSize(), client_max_packet_size);
  }
  if (packet.GetPayload().size() > 6) {
    ByteArray remaining_data =
        ByteArray(std::string(packet.GetPayload().AsStringView().substr(6)));
    socket_callback_.on_receive_cb(remaining_data);
  }
  WriteConnectionConfirm();
}

void ServerSocket::WriteConnectionConfirm() {
  state_ = State::kStateServerConfirm;
  absl::StatusOr<Packet> packet = Packet::CreateConnectionConfirmPacket(
      kProtocolVersion, max_packet_size_, initial_data_provider_->Provide());
  if (packet.ok()) {
    auto result = WriteControlPacket(packet.value());
    result.wait();
    auto status = result.get();
    if (!status.ok()) {
      DisconnectInternal(
          absl::InternalError("transmit of connection confirm failed"));
    }

    if (state_ != State::kStateServerConfirm) {
      socket_callback_.on_error_cb(
          absl::InternalError("unexpected transmit of connection confirm"));
    }
    state_ = State::kStateHandshakeCompleted;
    OnConnected(max_packet_size_);
  }
}

}  // namespace socket_abstraction
}  // namespace nearby
