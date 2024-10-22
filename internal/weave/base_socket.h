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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_BASE_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_BASE_SOCKET_H_

#include <deque>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/weave/connection.h"
#include "internal/weave/control_packet_write_request.h"
#include "internal/weave/message_write_request.h"
#include "internal/weave/packet.h"
#include "internal/weave/packet_sequence_number_generator.h"
#include "internal/weave/packetizer.h"
#include "internal/weave/socket_callback.h"

namespace nearby {
namespace weave {

// The BaseSocket class covers all common sending logic and management of
// control and message packets and provides a convenient public API for sending
// messages.
class BaseSocket {
 public:
  BaseSocket(const Connection& connection, SocketCallback&& callback);
  virtual ~BaseSocket();

  bool IsConnected() ABSL_LOCKS_EXCLUDED(mutex_);
  void Disconnect();
  nearby::Future<absl::Status> Write(ByteArray message);
  virtual void Connect() = 0;

 protected:
  void OnConnected(int new_max_packet_size);
  void DisconnectInternal(absl::Status status);
  // DisconnectQuietly() runs the socket disconnection code without sending an
  // error packet to the other side, and is needed for all disconnects, but
  // mainly for the ConnectionCallback::on_disconnect_cb.
  virtual void DisconnectQuietly();
  virtual void OnReceiveControlPacket(Packet packet) = 0;
  void WriteControlPacket(Packet packet);
  void OnReceiveDataPacket(Packet packet);
  void RunOnSocketThread(std::string name, Runnable&& runnable) {
    NEARBY_LOGS(INFO) << "RunOnSocketThread: " << name;
    executor_.Execute(name, std::move(runnable));
  }
  void ShutDown();

  // Test only functions.
  void AddControlPacket(Packet packet) ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    ControlPacketWriteRequest request(std::move(packet));
    control_request_queue_.push_back(std::move(request));
  }

  const Connection& GetConnection() const { return connection_; }
  const SocketCallback& GetSocketCallback() const { return socket_callback_; }

 private:
  enum class SocketConnectionState {
    kDisconnected,
    kDisconnecting,
    kConnected
  };

  bool IsRemotePacketCounterExpected(int counter);
  void TryWriteNextControl() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void TryWriteNextMessage() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnWriteRequestWriteComplete(absl::Status status)
      ABSL_LOCKS_EXCLUDED(executor_);
  void WritePacket(absl::StatusOr<Packet> packet);

  Mutex mutex_;
  // Messages and controls are in two separate queues to separate their control
  // flow and to make it easier to follow the logic of sending packets.
  std::deque<ControlPacketWriteRequest> control_request_queue_
      ABSL_GUARDED_BY(mutex_);
  std::deque<MessageWriteRequest> message_request_queue_
      ABSL_GUARDED_BY(mutex_);
  ControlPacketWriteRequest* current_control_ = nullptr;
  MessageWriteRequest* current_message_ = nullptr;
  SocketConnectionState state_ ABSL_GUARDED_BY(mutex_) =
      SocketConnectionState::kDisconnected;
  int max_packet_size_;
  Packetizer packetizer_;
  PacketSequenceNumberGenerator packet_counter_generator_;
  PacketSequenceNumberGenerator remote_packet_counter_generator_;
  SocketCallback socket_callback_;
  Connection& connection_;
  SingleThreadExecutor executor_;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_BASE_SOCKET_H_
