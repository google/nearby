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
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/socket_abstraction/connection.h"
#include "internal/socket_abstraction/packet.h"
#include "internal/socket_abstraction/socket.h"

namespace nearby {
namespace socket_abstraction {
namespace {

class FakeConnection : public Connection {
 public:
  explicit FakeConnection(int max_packet_size)
      : max_packet_size_(max_packet_size) {}
  void Initialize(ConnectionCallback callback) override {
    callback_ = callback;
  }

  int GetMaxPacketSize() override { return max_packet_size_; }
  void Transmit(ByteArray packet) override {
    packets_written_.push_back(packet);
    if (instant_transmit_) {
      callback_.on_transmit_cb(absl::OkStatus());
    }
  }
  void Close() override { open_ = false; }
  bool IsOpen() { return open_; }
  ByteArray PollWrittenPacket() {
    if (!NoMorePackets()) {
      auto front = packets_written_.front();
      packets_written_.erase(packets_written_.begin());
      return front;
    }
    NEARBY_LOGS(WARNING) << "No more packets";
    return ByteArray();
  }
  bool NoMorePackets() { return packets_written_.empty(); }

 protected:
  int max_packet_size_;
  ConnectionCallback callback_;
  std::vector<ByteArray> packets_written_;
  bool instant_transmit_ = true;
  bool open_ = false;
};

class FakeSocket : public BaseSocket {
 public:
  explicit FakeSocket(Connection* connection, SocketCallback&& socketCallback)
      : BaseSocket(connection, std::move(socketCallback)) {}
  MOCK_METHOD(void, Connect, (), (override));
  void OnReceiveControlPacket(Packet packet) override {
    control_packets_.push_back(packet);
  }
  // Proxies to internal protected methods
  void OnConnectedProxy(int max_packet_size) { OnConnected(max_packet_size); }
  void DisconnectQuietlyProxy() { DisconnectQuietly(); }
  std::future<absl::Status> WriteControlPacketProxy(Packet packet) {
    return WriteControlPacket(packet);
  }

  std::vector<Packet> control_packets_;
};

class BaseSocketTest : public ::testing::Test {
 public:
  BaseSocketTest()
      : connection_(new FakeConnection(20)),
        socket_(connection_, SocketCallback{
                                 .on_connected_cb =
                                     [this]() {
                                       MutexLock lock(&mutex_);
                                       connected_ = true;
                                     },
                                 .on_disconnected_cb =
                                     [this]() {
                                       MutexLock lock(&mutex_);
                                       connected_ = false;
                                     },
                                 .on_receive_cb =
                                     [this](ByteArray message) {
                                       MutexLock lock(&mutex_);
                                       messages_read_.push_back(message);
                                     },
                                 .on_error_cb =
                                     [](absl::Status status) {
                                       NEARBY_LOGS(ERROR) << status;
                                     },
                             }) {}
  void TearDown() override { delete connection_; }

 protected:
  FakeConnection* connection_;
  FakeSocket socket_;
  Mutex mutex_;
  bool connected_ = true;
  std::vector<ByteArray> messages_read_;
};

TEST_F(BaseSocketTest, TestConnectQueuedWrite) {
  std::future<absl::Status> result = socket_.Write(ByteArray("\x01\x02\x03"));
  EXPECT_TRUE(result.valid());
  socket_.OnConnectedProxy(3);
  // sleep for 10 ms to allow for status population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_OK(result.get());
  ByteArray packet = connection_->PollWrittenPacket();
  ByteArray second = connection_->PollWrittenPacket();
  Packet expected =
      Packet::CreateDataPacket(true, false, ByteArray("\x01\x02"));
  EXPECT_EQ(packet, expected.GetBytes());
  Packet expected2 = Packet::CreateDataPacket(false, true, ByteArray("\x03"));
  EXPECT_OK(expected2.SetPacketCounter(1));
  EXPECT_EQ(second, expected2.GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestDisconnect) {
  socket_.Disconnect();
  // sleep for 10 ms to allow for executor run to complete
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_->PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestDisconnectTwice) {
  socket_.Disconnect();
  socket_.Disconnect();
  // sleep for 10 ms to allow for executor run to complete
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_->PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestOnDisconnected) {
  socket_.OnConnectedProxy(3);
  // sleep for 10 ms to allow for connection status to propagate
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(socket_.IsConnected());
  socket_.DisconnectQuietlyProxy();
  EXPECT_FALSE(socket_.IsConnected());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteControlPacket) {
  std::future<absl::Status> status =
      socket_.WriteControlPacketProxy(Packet::CreateErrorPacket());
  EXPECT_OK(status.get());
  EXPECT_EQ(connection_->PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  Packet second = Packet::CreateErrorPacket();
  EXPECT_OK(second.SetPacketCounter(1));
  status = socket_.WriteControlPacketProxy(Packet::CreateErrorPacket());
  EXPECT_OK(status.get());
  EXPECT_EQ(connection_->PollWrittenPacket(), second.GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteOnePacket) {
  socket_.OnConnectedProxy(3);
  std::future<absl::Status> status = socket_.Write(ByteArray("\x01\x02"));
  EXPECT_OK(status.get());
  EXPECT_EQ(
      connection_->PollWrittenPacket(),
      Packet::CreateDataPacket(true, true, ByteArray("\x01\x02")).GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteThreePackets) {
  socket_.OnConnectedProxy(3);
  std::future<absl::Status> status =
      socket_.Write(ByteArray("\x01\x02\x03\x04\x05\x06"));
  EXPECT_OK(status.get());
  EXPECT_EQ(
      connection_->PollWrittenPacket(),
      Packet::CreateDataPacket(true, false, ByteArray("\x01\x02")).GetBytes());
  Packet second = Packet::CreateDataPacket(false, false, ByteArray("\x03\x04"));
  EXPECT_OK(second.SetPacketCounter(1));
  EXPECT_EQ(connection_->PollWrittenPacket(), second.GetBytes());
  Packet third = Packet::CreateDataPacket(false, true, ByteArray("\x05\x06"));
  EXPECT_OK(third.SetPacketCounter(2));
  EXPECT_EQ(connection_->PollWrittenPacket(), third.GetBytes());
  EXPECT_TRUE(connection_->NoMorePackets());
}

TEST_F(BaseSocketTest, TestWritePacketCounterRollover) {
  socket_.OnConnectedProxy(3);
  for (int i = 0; i <= Packet::kMaxPacketCounter; i++) {
    Packet packet = Packet::CreateDataPacket(true, true, ByteArray("\x01"));
    EXPECT_OK(packet.SetPacketCounter(i));
    std::future<absl::Status> result = socket_.Write(ByteArray("\x01"));
    EXPECT_OK(result.get());
    EXPECT_EQ(connection_->PollWrittenPacket(), packet.GetBytes());
  }
  Packet packet = Packet::CreateDataPacket(true, true, ByteArray("\x01"));
  std::future<absl::Status> result = socket_.Write(ByteArray("\x01"));
  EXPECT_OK(result.get());
  EXPECT_EQ(connection_->PollWrittenPacket(), packet.GetBytes());
}

// TODO(b/269783814): add the rest of the tests from the Java implementation

}  // namespace
}  // namespace socket_abstraction
}  // namespace nearby
