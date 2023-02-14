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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_CLIENT_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_CLIENT_SOCKET_H_

#include "internal/socket_abstraction/base_socket.h"
#include "internal/socket_abstraction/connection.h"
#include "internal/socket_abstraction/socket.h"
#include "internal/socket_abstraction/v1/initial_data_provider.h"

namespace nearby {
namespace socket_abstraction {

class ClientSocket : public BaseSocket {
 public:
  ClientSocket(Connection* connection, SocketCallback socket_callback);
  ClientSocket(Connection* connection, SocketCallback socket_callback,
               InitialDataProvider& initial_data_provider);
  ~ClientSocket() override;
  void Connect() override;

 protected:
  bool IsConnectingOrConnected() override;
  void DisconnectQuietly() override;
  void OnReceiveControlPacket(Packet packet) override;

 private:
  static constexpr int kProtocolVersion = 0b0001;
  enum class State {
    kStateDisconnected = 0,
    kStateClientConnectionRequest = 1,
    kStateServerConfirm = 2,
    kStateHandshakeCompleted = 3,
  };
  State state_ = State::kStateDisconnected;
  InitialDataProvider* initial_data_provider_;
  bool provider_needs_delete_ = false;
  SingleThreadExecutor executor_;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_CLIENT_SOCKET_H_
