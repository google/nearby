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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_SERVER_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_SERVER_SOCKET_H_

#include <memory>

#include "internal/weave/base_socket.h"
#include "internal/weave/connection.h"
#include "internal/weave/socket_callback.h"

namespace nearby {
namespace weave {

class ServerSocket : public BaseSocket {
 public:
  ServerSocket(const Connection& connection, SocketCallback&& socket_callback);
  ~ServerSocket() override;
  ServerSocket(const ServerSocket&) = delete;
  ServerSocket& operator=(const ServerSocket&) = delete;
  void Connect() override {}

 protected:
  void DisconnectQuietly() override;
  void OnReceiveControlPacket(Packet packet) override;

 private:
  enum class State {
    kDisconnected = 0,
    kClientConnectionRequest = 1,
    kServerConfirm = 2,
    kHandshakeCompleted = 3,
  };

  void WriteConnectionConfirm();

  State state_ = State::kClientConnectionRequest;
  int max_packet_size_ = 0;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_SERVER_SOCKET_H_
