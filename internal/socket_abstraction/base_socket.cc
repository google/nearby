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

#include "internal/socket_abstraction/base_socket.h"

// NOLINTNEXTLINE
#include <future>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/socket_abstraction/connection.h"
#include "internal/socket_abstraction/message_write_request.h"
#include "internal/socket_abstraction/packet.h"
#include "internal/socket_abstraction/single_packet_write_request.h"
#include "internal/socket_abstraction/write_request.h"

namespace nearby {
namespace socket_abstraction {

// WriteRequestExecutor
BaseSocket::WriteRequestExecutor::~WriteRequestExecutor() {
  CountDownLatch latch(1);
  executor_.Execute("Cleaner", [&latch]() { latch.CountDown(); });
  latch.Await();
  executor_.Shutdown();
}

void BaseSocket::WriteRequestExecutor::TryWriteExternal() {
  executor_.Execute("TryWrite", [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                    executor_) { TryWrite(); });
}

void BaseSocket::WriteRequestExecutor::TryWrite() {
  if (current_ != nullptr) {
    return;
  }
  WriteRequest* request = controls_.begin().base();
  if (controls_.begin() == controls_.end()) {
    request = nullptr;
  }
  if (request != nullptr) {
    NEARBY_LOGS(INFO) << "picked control";
    NEARBY_LOGS(INFO) << "request finished: " << request->IsFinished();
  }
  if (request == nullptr && socket_->IsConnected()) {
    NEARBY_LOGS(INFO) << "picking message";
    if (messages_.begin() != messages_.end()) {
      request = messages_.begin().base();
    }
  } else {
    ResetFirstMessageIfStarted();
  }
  if (request != nullptr) {
    if (request->IsFinished()) {
      NEARBY_LOGS(INFO) << "request finished";
      return;
    }
    current_ = request;
  }
  if (current_ != nullptr) {
    absl::StatusOr<Packet> packet =
        current_->NextPacket(socket_->max_packet_size_);
    if (packet.ok()) {
      if (!packet.value()
               .SetPacketCounter(socket_->packet_counter_generator_.Next())
               .ok()) {
        return;
      }
      NEARBY_LOGS(INFO) << "transmitting pkt";
      socket_->connection_->Transmit(packet.value().GetBytes());
    } else {
      NEARBY_LOGS(WARNING) << packet.status();
      return;
    }
  }
}

void BaseSocket::WriteRequestExecutor::OnWriteResult(absl::Status status) {
  NEARBY_LOGS(INFO) << "OnWriteResult";
  executor_.Execute(
      "OnWriteResult",
      [this, status]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
        if (current_ == nullptr) {
          return;
        }
        if (current_->IsFinished()) {
          NEARBY_LOGS(INFO) << "OnWriteResult current finished";
          current_->SetFuture(status);
          if (current_ == messages_.begin().base()) {
            NEARBY_LOGS(INFO) << "remove message";
            messages_.erase(messages_.begin());
          } else if (current_ == controls_.begin().base()) {
            NEARBY_LOGS(INFO) << "remove control";
            controls_.erase(controls_.begin());
          }
        }
        NEARBY_LOGS(INFO) << "controls size: " << controls_.size();
        NEARBY_LOGS(INFO) << "messages size: " << messages_.size();
        current_ = nullptr;
        TryWrite();
      });
}

namespace {

std::future<absl::Status> QueueMessageInternal(
    ByteArray message, std::vector<MessageWriteRequest>& messages,
    Mutex& to_lock) {
  MutexLock lock(&to_lock);
  messages.push_back(MessageWriteRequest(message));
  return messages.back().GetFutureResult();
}

std::future<absl::Status> QueueControlInternal(
    Packet controlPacket, std::vector<SinglePacketWriteRequest>& messages,
    Mutex& to_lock) {
  MutexLock lock(&to_lock);
  messages.push_back(SinglePacketWriteRequest(controlPacket));
  return messages.back().GetFutureResult();
}

}  // namespace

std::future<absl::Status> BaseSocket::WriteRequestExecutor::Queue(
    ByteArray message) {
  std::future<absl::Status> ret =
      QueueMessageInternal(message, messages_, mutex_);

  executor_.Execute("TryWrite", [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                    executor_) { TryWrite(); });
  return ret;
}

std::future<absl::Status> BaseSocket::WriteRequestExecutor::Queue(
    Packet packet) {
  std::future<absl::Status> ret =
      QueueControlInternal(packet, controls_, mutex_);

  executor_.Execute("TryWrite", [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                    executor_) { TryWrite(); });
  return ret;
}

void BaseSocket::WriteRequestExecutor::ResetFirstMessageIfStarted() {
  auto latest = messages_.begin();
  if (latest != messages_.end() && latest->IsStarted()) {
    latest->Reset();
  }
}

// BaseSocket implementation
bool BaseSocket::IsRemotePacketCounterExpected(Packet packet) {
  int counter = packet.GetPacketCounter();
  int expectedPacketCounter = remote_packet_counter_generator_.Next();
  if (counter == expectedPacketCounter) {
    return true;
  } else {
    socket_callback_.on_error_cb(absl::DataLossError(absl::StrFormat(
        "expected remote packet counter %d for packet %s but got %d",
        expectedPacketCounter, packet.ToString(), counter)));
    return false;
  }
}

BaseSocket::BaseSocket(Connection* connection, SocketCallback&& callback)
    : write_request_executor_(WriteRequestExecutor(this)) {
  connection_ = connection;
  socket_callback_ = std::move(callback);
  connection_callback_ = {
      .on_transmit_cb =
          [this](absl::Status status) {
            write_request_executor_.OnWriteResult(status);
            if (!status.ok()) {
              DisconnectInternal(status);
            }
          },
      .on_remote_transmit_cb =
          [this](ByteArray array) {
            if (array.Empty()) {
              DisconnectInternal(absl::InvalidArgumentError("Empty packet!"));
              return;
            }
            Packet packet = Packet::FromBytes(array);
            bool isRemotePacketCounterExpected =
                IsRemotePacketCounterExpected(packet);
            if (packet.IsControlPacket()) {
              if (!isRemotePacketCounterExpected) {
                remote_packet_counter_generator_.Update(
                    packet.GetPacketCounter());
              }
              OnReceiveControlPacket(packet);
            } else {
              if (isRemotePacketCounterExpected) {
                OnReceiveDataPacket(packet);
              } else {
                Disconnect();
              }
            }
          },
      .on_disconnected_cb = [this]() { DisconnectQuietly(); }};
  connection_->Initialize(connection_callback_);
}

BaseSocket::~BaseSocket() {
  CountDownLatch latch(1);
  executor_.Execute("Cleaner", [&latch]() { latch.CountDown(); });
  latch.Await();
  executor_.Shutdown();
}

void BaseSocket::Disconnect() {
  if (!is_disconnecting_) {
    is_disconnecting_ = true;
    write_request_executor_.Queue(Packet::CreateErrorPacket()).wait();
    DisconnectQuietly();
  }
}

void BaseSocket::DisconnectQuietly() {
  bool wasConnectingOrConnected = IsConnectingOrConnected();
  connected_ = false;
  if (wasConnectingOrConnected) {
    socket_callback_.on_disconnected_cb();
  }
  executor_.Execute("DisconnectQuietly", [this]() {
    write_request_executor_.ResetFirstMessageIfStarted();
    packetizer_.Reset();
    packet_counter_generator_.Reset();
    remote_packet_counter_generator_.Reset();
  });
}

void BaseSocket::OnReceiveDataPacket(Packet packet) {
  absl::StatusOr<ByteArray> message = packetizer_.Join(packet);
  if (message.ok()) {
    socket_callback_.on_receive_cb(message.value());
  } else {
    DisconnectInternal(message.status());
  }
}

std::future<absl::Status> BaseSocket::Write(ByteArray message) {
  return write_request_executor_.Queue(message);
}

std::future<absl::Status> BaseSocket::WriteControlPacket(Packet packet) {
  return write_request_executor_.Queue(packet);
}

void BaseSocket::DisconnectInternal(absl::Status status) {
  Disconnect();
  socket_callback_.on_error_cb(status);
}

bool BaseSocket::IsConnected() { return connected_; }

bool BaseSocket::IsConnectingOrConnected() { return connected_; }

void BaseSocket::OnConnected(int new_max_packet_size) {
  max_packet_size_ = new_max_packet_size;
  bool was_connected = IsConnected();
  connected_ = true;
  is_disconnecting_ = false;
  if (!was_connected) {
    socket_callback_.on_connected_cb();
  }
  write_request_executor_.TryWriteExternal();
}

}  // namespace socket_abstraction
}  // namespace nearby
