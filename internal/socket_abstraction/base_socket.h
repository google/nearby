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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_BASE_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_BASE_SOCKET_H_

// NOLINTNEXTLINE
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/socket_abstraction/connection.h"
#include "internal/socket_abstraction/message_write_request.h"
#include "internal/socket_abstraction/packet.h"
#include "internal/socket_abstraction/packet_counter_generator.h"
#include "internal/socket_abstraction/packetizer.h"
#include "internal/socket_abstraction/single_packet_write_request.h"
#include "internal/socket_abstraction/socket.h"
#include "internal/socket_abstraction/write_request.h"

namespace nearby {
namespace socket_abstraction {

class BaseSocket : public Socket {
 public:
  BaseSocket(Connection* connection, SocketCallback&& callback);
  ~BaseSocket() override;
  bool IsConnected() override;
  void Disconnect() override;
  std::future<absl::Status> Write(ByteArray message) override;

 protected:
  void OnConnected(int new_max_packet_size);
  void DisconnectInternal(absl::Status status);
  virtual bool IsConnectingOrConnected();
  virtual void DisconnectQuietly();
  virtual void OnReceiveControlPacket(Packet packet) {}
  virtual std::future<absl::Status> WriteControlPacket(Packet packet);
  void OnReceiveDataPacket(Packet packet);
  Connection* connection_;
  SocketCallback socket_callback_;
  SingleThreadExecutor executor_;

 private:
  class WriteRequestExecutor {
   public:
    explicit WriteRequestExecutor(BaseSocket* socket) : socket_(socket) {}
    ~WriteRequestExecutor() ABSL_LOCKS_EXCLUDED(executor_);
    void TryWriteExternal() ABSL_LOCKS_EXCLUDED(executor_);
    void OnWriteResult(absl::Status status) ABSL_LOCKS_EXCLUDED(executor_);
    std::future<absl::Status> Queue(ByteArray message)
        ABSL_LOCKS_EXCLUDED(mutex_, executor_);
    std::future<absl::Status> Queue(Packet packet)
        ABSL_LOCKS_EXCLUDED(mutex_, executor_);
    void ResetFirstMessageIfStarted();

   private:
    void TryWrite() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
    BaseSocket* socket_;
    Mutex mutex_;
    std::vector<SinglePacketWriteRequest> controls_ = {};
    std::vector<MessageWriteRequest> messages_ = {};
    WriteRequest* current_ = nullptr;
    SingleThreadExecutor executor_;
  };
  bool IsRemotePacketCounterExpected(Packet packet);
  ConnectionCallback connection_callback_;
  Packetizer packetizer_ = {};
  PacketCounterGenerator packet_counter_generator_;
  PacketCounterGenerator remote_packet_counter_generator_;
  bool connected_ = false;
  bool is_disconnecting_ = false;
  int max_packet_size_;
  WriteRequestExecutor write_request_executor_;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_BASE_SOCKET_H_
