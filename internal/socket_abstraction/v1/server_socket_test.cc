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

// NOLINTNEXTLINE
#include <future>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace socket_abstraction {
namespace {

constexpr int kClientMaxPacketSize = 4;
constexpr int kServerMaxPacketSize = 3;
constexpr int kProtocolVersion = 1;

class TestServerSocket : public ServerSocket {
 public:
  TestServerSocket(Connection* connection, SocketCallback callback)
      : ServerSocket(connection, callback) {}
  void OnReceiveControlPacketProxy(Packet packet) {
    OnReceiveControlPacket(packet);
  }
};

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
  void SetMaxPacketSize(int packet_size) { max_packet_size_ = packet_size; }
  void Close() override { open_ = false; }
  bool IsOpen() { return open_; }
  ByteArray PollWrittenPacket() {
    // Make sure all callbacks finish up before we try polling a packet
    absl::SleepFor(absl::Milliseconds(10));
    auto front = packets_written_.erase(packets_written_.begin());
    return *front.base();
  }
  bool NoMorePackets() { return packets_written_.empty(); }
  void SetInstantTransmit(bool instant_transmit) {
    instant_transmit_ = instant_transmit;
  }
  void OnTransmitProxy(absl::Status status) {
    callback_.on_transmit_cb(status);
  }

 protected:
  int max_packet_size_;
  ConnectionCallback callback_;
  std::vector<ByteArray> packets_written_;
  bool instant_transmit_ = true;
  bool open_ = false;
};

class ServerSocketTest : public ::testing::Test {
 public:
  ServerSocketTest()
      : connection_(FakeConnection(kServerMaxPacketSize)),
        socket_(TestServerSocket(
            &connection_,
            SocketCallback{
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
                    [](absl::Status status) { NEARBY_LOGS(ERROR) << status; },
            })) {}
  void SetUp() override { EXPECT_FALSE(socket_.IsConnected()); }

  void TearDown() override {
    EXPECT_EQ(socket_.IsConnected(), expect_connected_);
  }
  void RunConnect(int client_size, int server_size, int expected_size,
                  ByteArray initial_data) {
    connection_.SetMaxPacketSize(server_size);
    socket_.OnReceiveControlPacketProxy(
        Packet::CreateConnectionRequestPacket(kProtocolVersion - 1,
                                              kProtocolVersion + 1, client_size,
                                              initial_data)
            .value());
    Packet packet = Packet::FromBytes(connection_.PollWrittenPacket());
    EXPECT_TRUE(packet.IsControlPacket());
    EXPECT_EQ(packet.GetControlCommandNumber(),
              Packet::kControlConnectionConfirm);
    EXPECT_EQ(packet.GetPacketCounter(), 0);
    int16_t agreed_protocol_version =
        ((packet.GetPayload().data()[1] << 8) | packet.GetPayload().data()[0]) &
        (int16_t)0xFFFF;
    EXPECT_EQ(agreed_protocol_version, kProtocolVersion);
    int pkt_size =
        ((packet.GetPayload().data()[3] << 8) | packet.GetPayload().data()[2]) &
        (int16_t)0xFFFF;
    EXPECT_EQ(pkt_size, expected_size);
    int expected_payload_size = expected_size - 1;
    std::future<absl::Status> status =
        socket_.Write(ByteArray(expected_payload_size));
    status.wait();
    EXPECT_OK(status.get());
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
    status = socket_.Write(ByteArray(expected_payload_size + 1));
    status.wait();
    EXPECT_OK(status.get());
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
  }
  FakeConnection connection_;
  TestServerSocket socket_;
  Mutex mutex_;
  bool connected_ = true;
  std::vector<ByteArray> messages_read_;
  bool expect_connected_ = true;
};

TEST_F(ServerSocketTest, TestClientServerPacketSizeSame) {
  RunConnect(2, 2, 2, ByteArray());
}

TEST_F(ServerSocketTest, TestClientPacketSizeLower) {
  RunConnect(3, 4, 3, ByteArray());
}

TEST_F(ServerSocketTest, TestServerPacketSizeLower) {
  RunConnect(5, 4, 4, ByteArray());
}

TEST_F(ServerSocketTest, TestClientMaxPacketSizeZero) {
  RunConnect(0, 2, 2, ByteArray());
}

TEST_F(ServerSocketTest, TestConnectInitialData) {
  ByteArray initial_data("12");
  RunConnect(2, 2, 2, initial_data);
  EXPECT_EQ(messages_read_.size(), 1);
  EXPECT_EQ(messages_read_[0], initial_data);
}

TEST_F(ServerSocketTest, TestDisconnect) {
  RunConnect(2, 2, 2, ByteArray());
  socket_.Disconnect();
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestDisconnectConnect) {
  RunConnect(2, 2, 2, ByteArray());
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  RunConnect(2, 2, 2, ByteArray());
}

TEST_F(ServerSocketTest, TestDisconnectTwice) {
  RunConnect(2, 2, 2, ByteArray());
  socket_.Disconnect();
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  socket_.Disconnect();
  EXPECT_TRUE(connection_.NoMorePackets());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestNoConnectionRequestWrongCommand) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(0, 0, ByteArray()).value());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestUnsupportProtocolVersionTooLow) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(kProtocolVersion - 2,
                                            kProtocolVersion - 1,
                                            kClientMaxPacketSize, ByteArray())
          .value());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestUnsupportProtocolVersionTooHigh) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(kProtocolVersion + 1,
                                            kProtocolVersion + 2,
                                            kClientMaxPacketSize, ByteArray())
          .value());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestInitiateConnect) {
  expect_connected_ = false;
  socket_.Connect();
}

}  // namespace
}  // namespace socket_abstraction
}  // namespace nearby
