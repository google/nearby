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

#include "internal/weave/base_socket.h"

#include <deque>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/weave/connection.h"
#include "internal/weave/control_packet_write_request.h"
#include "internal/weave/message_write_request.h"
#include "internal/weave/packet.h"
#include "internal/weave/socket_callback.h"

namespace nearby {
namespace weave {

BaseSocket::BaseSocket(const Connection& connection, SocketCallback&& callback)
    : socket_callback_(std::move(callback)),
      connection_(const_cast<Connection&>(connection)) {
  connection_.Initialize(
      {.on_transmit_cb =
           [this](absl::Status status) {
             OnWriteRequestWriteComplete(status);
             if (!status.ok()) {
               DisconnectInternal(status);
             }
           },
       .on_remote_transmit_cb =
           [this](std::string message) {
             if (message.empty()) {
               DisconnectInternal(absl::InvalidArgumentError("Empty packet!"));
               return;
             }
             absl::StatusOr<Packet> packet{
                 Packet::FromBytes(ByteArray(message))};
             if (!packet.ok()) {
               DisconnectInternal(packet.status());
               return;
             }
             bool isRemotePacketCounterExpected =
                 IsRemotePacketCounterExpected(packet->GetPacketCounter());
             if (packet->IsControlPacket()) {
               if (!isRemotePacketCounterExpected) {
                 // Increment the counter by 1, ignoring the result.
                 remote_packet_counter_generator_.Next();
               }
               OnReceiveControlPacket(std::move(*packet));
             } else {
               if (isRemotePacketCounterExpected) {
                 OnReceiveDataPacket(std::move(*packet));
               } else {
                 Disconnect();
               }
             }
           },
       .on_disconnected_cb = [this]() { DisconnectQuietly(); }});
  max_packet_size_ = connection_.GetMaxPacketSize();
}

BaseSocket::~BaseSocket() {
  NEARBY_LOGS(INFO) << "~BaseSocket";
  ShutDown();
}

void BaseSocket::ShutDown() {
  executor_.Shutdown();
  NEARBY_LOGS(INFO) << "BaseSocket gone.";
}

void BaseSocket::TryWriteNextControl() {
  MutexLock lock(&mutex_);
  if (current_control_ == nullptr) {
    if (!control_request_queue_.empty()) {
      current_control_ = &control_request_queue_.front();
    }
  }
  // We should have a control packet to write at this point. If we don't, abort.
  if (current_control_ == nullptr) {
    return;
  }
  // We need to do this because if a control packet is being sent, it is
  // one of three packets. ConnectionRequest, ConnectionConfirm, or Error.
  // In any case, we should not have any messages in the queue from the previous
  // connection.
  current_message_ = nullptr;
  message_request_queue_.clear();
  WritePacket(current_control_->NextPacket(max_packet_size_));
}

void BaseSocket::TryWriteNextMessage() {
  bool control_queue_empty = false;
  {
    MutexLock lock(&mutex_);
    control_queue_empty = control_request_queue_.empty();
  }
  if (current_control_ != nullptr || !control_queue_empty) {
    // We should only be writing one packet.
    TryWriteNextControl();
    return;
  }
  bool connected = IsConnected();
  MutexLock lock(&mutex_);
  if (current_message_ == nullptr) {
    if (!message_request_queue_.empty()) {
      current_message_ = &message_request_queue_.front();
    }
  }
  if (current_message_ == nullptr || current_message_->IsFinished() ||
      !connected) {
    return;
  }
  WritePacket(current_message_->NextPacket(max_packet_size_));
}

void BaseSocket::WritePacket(absl::StatusOr<Packet> packet) {
  if (!packet.ok()) {
    NEARBY_LOGS(WARNING) << "Packet status:" << packet.status();
    return;
  }
  CHECK(packet->SetPacketCounter(packet_counter_generator_.Next()).ok());
  NEARBY_LOGS(INFO) << "transmitting packet";
  connection_.Transmit(packet->GetBytes());
}

void BaseSocket::OnWriteRequestWriteComplete(absl::Status status) {
  RunOnSocketThread(
      "OnWriteRequestWriteComplete",
      [this, status]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_)
          ABSL_LOCKS_EXCLUDED(mutex_) mutable {
            {
              MutexLock lock(&mutex_);
              if (current_control_ != nullptr) {
                current_control_ = nullptr;
                control_request_queue_.pop_front();
              } else if (current_message_ != nullptr) {
                NEARBY_LOGS(INFO) << "OnWriteResult current is not null";
                if (current_message_->IsFinished()) {
                  NEARBY_LOGS(INFO) << "OnWriteResult current finished";
                  current_message_->SetWriteStatus(status);
                  if (!message_request_queue_.empty() &&
                      current_message_ == &message_request_queue_.front()) {
                    NEARBY_LOGS(INFO) << "remove message";
                    message_request_queue_.pop_front();
                    current_message_ = nullptr;
                  }
                }
              }
            }
            TryWriteNextMessage();
          });
}

bool BaseSocket::IsRemotePacketCounterExpected(int counter) {
  int expectedPacketCounter = remote_packet_counter_generator_.Next();
  if (counter == expectedPacketCounter) {
    return true;
  }
  socket_callback_.on_error_cb(absl::DataLossError(
      absl::StrFormat("expected remote packet counter %d for packet but got %d",
                      expectedPacketCounter, counter)));
  return false;
}

void BaseSocket::Disconnect() {
  RunOnSocketThread("Disconnect", [this]() {
    bool is_disconnecting_or_disconnected = false;
    {
      MutexLock lock(&mutex_);
      is_disconnecting_or_disconnected =
          state_ == SocketConnectionState::kDisconnecting ||
          state_ == SocketConnectionState::kDisconnected;
    }
    if (!is_disconnecting_or_disconnected) {
      WriteControlPacket(Packet::CreateErrorPacket());
      {
        MutexLock lock(&mutex_);
        current_message_ = nullptr;
        message_request_queue_.clear();
        state_ = SocketConnectionState::kDisconnecting;
      }
      DisconnectQuietly();
    }
  });
}

void BaseSocket::DisconnectQuietly() {
  RunOnSocketThread("ResetDisconnectQuietly",
                    [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_)
                        ABSL_LOCKS_EXCLUDED(mutex_) {
                          bool was_connected = false;
                          {
                            MutexLock lock(&mutex_);
                            was_connected =
                                state_ == SocketConnectionState::kDisconnecting;
                          }

                          if (was_connected) {
                            socket_callback_.on_disconnected_cb();
                          }
                          packetizer_.Reset();
                          packet_counter_generator_.Reset();
                          remote_packet_counter_generator_.Reset();
                          // Dump message and control queue.
                          {
                            MutexLock lock(&mutex_);
                            message_request_queue_.clear();
                            control_request_queue_.clear();
                            current_control_ = nullptr;
                            current_message_ = nullptr;
                            state_ = SocketConnectionState::kDisconnected;
                          }
                          NEARBY_LOGS(INFO) << "Socket now disconnected.";
                        });
  NEARBY_LOGS(INFO) << "scheduled reset";
}

void BaseSocket::OnReceiveDataPacket(Packet packet) {
  absl::Status packet_status = packetizer_.AddPacket(std::move(packet));
  if (!packet_status.ok()) {
    DisconnectInternal(packet_status);
    return;
  }
  absl::StatusOr<ByteArray> message = packetizer_.TakeMessage();
  if (!message.ok()) {
    DisconnectInternal(message.status());
    return;
  }
  socket_callback_.on_receive_cb(message->string_data());
}

nearby::Future<absl::Status> BaseSocket::Write(ByteArray message) {
  MessageWriteRequest request = MessageWriteRequest(message.string_data());
  nearby::Future<absl::Status> ret = request.GetWriteStatusFuture();

  RunOnSocketThread(
      "TryWriteMessage",
      [&, request = std::move(request)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
            {
              MutexLock lock(&mutex_);
              message_request_queue_.push_back(std::move(request));
            }
            TryWriteNextMessage();
          });
  return ret;
}

void BaseSocket::WriteControlPacket(Packet packet) {
  ControlPacketWriteRequest request =
      ControlPacketWriteRequest(std::move(packet));
  RunOnSocketThread(
      "TryWriteNextControl",
      [&, request = std::move(request)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
            {
              MutexLock lock(&mutex_);
              control_request_queue_.push_back(std::move(request));
            }
            TryWriteNextControl();
          });
  NEARBY_LOGS(INFO) << "Scheduled TryWriteControl";
}

void BaseSocket::DisconnectInternal(absl::Status status) {
  socket_callback_.on_error_cb(status);
  Disconnect();
}

bool BaseSocket::IsConnected() {
  MutexLock lock(&mutex_);
  return state_ == SocketConnectionState::kConnected;
}

void BaseSocket::OnConnected(int new_max_packet_size) {
  RunOnSocketThread("TryWriteOnConnected",
                    [this, new_max_packet_size]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
                          max_packet_size_ = new_max_packet_size;
                          bool was_connected = IsConnected();
                          if (!was_connected) {
                            {
                              MutexLock lock(&mutex_);
                              state_ = SocketConnectionState::kConnected;
                            }
                            socket_callback_.on_connected_cb();
                          }
                          TryWriteNextMessage();
                        });
}

}  // namespace weave
}  // namespace nearby
