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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_CLIENT_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_CLIENT_SOCKET_H_

#include <memory>

#include "internal/platform/single_thread_executor.h"
#include "internal/weave/base_socket.h"
#include "internal/weave/connection.h"
#include "internal/weave/packet.h"
#include "internal/weave/socket_callback.h"
#include "internal/weave/sockets/initial_data_provider.h"

namespace nearby {
namespace weave {

// The Weave Client socket class. This socket implements handling of control
// packets in accordance with the Weave handshake.
class ClientSocket : public BaseSocket {
 public:
  ClientSocket(const Connection& connection, SocketCallback&& socket_callback);
  // Note that when using this constructor, ClientSocket will take ownership of
  // the InitialDataProvider instance.
  ClientSocket(const Connection& connection, SocketCallback&& socket_callback,
               std::unique_ptr<InitialDataProvider> initial_data_provider);
  ~ClientSocket() override;
  void Connect() override;

 protected:
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
  std::unique_ptr<InitialDataProvider> initial_data_provider_;
  SingleThreadExecutor executor_;
  State state_ = State::kStateDisconnected;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_CLIENT_SOCKET_H_
