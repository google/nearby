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

#include "internal/weave/sockets/server_socket.h"

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"

namespace nearby {
namespace weave {
namespace {

using ::testing::status::StatusIs;

constexpr int kClientMaxPacketSize = 4;
constexpr int kServerMaxPacketSize = 3;
constexpr int kProtocolVersion = 1;

class TestServerSocket : public ServerSocket {
 public:
  TestServerSocket(const Connection& connection, SocketCallback callback)
      : ServerSocket(connection, std::move(callback)) {}
  void OnReceiveControlPacketProxy(Packet packet) {
    OnReceiveControlPacket(std::move(packet));
  }
};

class FakeConnection : public Connection {
 public:
  explicit FakeConnection(int max_packet_size)
      : max_packet_size_(max_packet_size) {}
  void Initialize(ConnectionCallback callback) override {
    callback_ = std::move(callback);
  }

  int GetMaxPacketSize() const override { return max_packet_size_; }
  void Transmit(std::string packet) override {
    absl::MutexLock lock(&mutex_);
    packets_written_.push_back(packet);
    if (instant_transmit_) {
      callback_.on_transmit_cb(absl::OkStatus());
    }
  }
  void SetMaxPacketSize(int packet_size) { max_packet_size_ = packet_size; }
  void Close() override { open_ = false; }
  bool IsOpen() { return open_; }
  std::string PollWrittenPacket() {
    absl::SleepFor(absl::Milliseconds(10));
    if (!NoMorePackets()) {
      absl::MutexLock lock(&mutex_);
      auto front = packets_written_.front();
      packets_written_.erase(packets_written_.begin());
      return front;
    }
    NEARBY_LOGS(WARNING) << "No more packets";
    return "";
  }
  bool NoMorePackets() {
    absl::MutexLock lock(&mutex_);
    return packets_written_.empty();
  }
  void SetInstantTransmit(bool instant_transmit) {
    instant_transmit_ = instant_transmit;
  }
  void OnTransmitProxy(absl::Status status) {
    callback_.on_transmit_cb(status);
  }

 protected:
  int max_packet_size_;
  ConnectionCallback callback_;
  absl::Mutex mutex_;
  std::vector<std::string> packets_written_ ABSL_GUARDED_BY(mutex_);
  bool instant_transmit_ = true;
  bool open_ = false;
};

class ServerSocketTest : public ::testing::Test {
 public:
  ServerSocketTest()
      : connection_(FakeConnection(kServerMaxPacketSize)),
        socket_(TestServerSocket(connection_,
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
                                         [this](std::string message) {
                                           MutexLock lock(&mutex_);
                                           messages_read_.push_back(message);
                                         },
                                     .on_error_cb =
                                         [this](absl::Status status) {
                                           last_error_ = status;
                                           NEARBY_LOGS(ERROR) << status;
                                         },
                                 })) {}
  void SetUp() override { EXPECT_FALSE(socket_.IsConnected()); }

  void TearDown() override {
    absl::SleepFor(absl::Milliseconds(10));
    EXPECT_EQ(socket_.IsConnected(), expect_connected_);
  }
  void RunConnect(int client_max_packet_size, int server_max_packet_size,
                  int expected_size, absl::string_view initial_data) {
    // Set the max packet size of the connection.
    connection_.SetMaxPacketSize(server_max_packet_size);
    // Send a connection request packet to the server socket with the client's
    // max packet size.
    socket_.OnReceiveControlPacketProxy(
        Packet::CreateConnectionRequestPacket(
            kProtocolVersion - 1, kProtocolVersion + 1, client_max_packet_size,
            initial_data)
            .value());
    // Check for a connection confirm packet and that the protocol version is 1.
    auto packet = Packet::FromBytes(ByteArray(connection_.PollWrittenPacket()));
    ASSERT_OK(packet);
    EXPECT_TRUE(packet->IsControlPacket());
    EXPECT_EQ(packet->GetControlCommandNumber(),
              Packet::ControlPacketType::kControlConnectionConfirm);
    EXPECT_EQ(packet->GetPacketCounter(), 0);
    int16_t agreed_protocol_version = ((packet->GetPayload().data()[0] << 8) |
                                       packet->GetPayload().data()[1]) &
                                      (int16_t)0xFFFF;
    EXPECT_EQ(agreed_protocol_version, kProtocolVersion);
    // Check packet size for the expected size.
    int packet_size = ((packet->GetPayload().data()[2] << 8) |
                       packet->GetPayload().data()[3]) &
                      (int16_t)0xFFFF;
    EXPECT_EQ(packet_size, expected_size);
    // Now write a data packet of the expected payload size.
    int expected_payload_size = expected_size - 1;
    auto status = socket_.Write(ByteArray(expected_payload_size));
    EXPECT_OK(status.Get().GetResult());
    // Make sure that the packet was actually written.
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
    // Write two packets, one of the expected size, and one of size 1.
    status = socket_.Write(ByteArray(expected_payload_size + 1));
    EXPECT_OK(status.Get().GetResult());
    // Check for two packets.
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
  }
  FakeConnection connection_;
  Mutex mutex_;
  bool connected_ = true;
  std::vector<std::string> messages_read_;
  absl::Status last_error_;
  bool expect_connected_ = true;
  TestServerSocket socket_;
};

TEST_F(ServerSocketTest, TestClientServerPacketSizeSame) {
  RunConnect(/*client_max_packet_size=*/2, /*server_max_packet_size=*/2,
             /*expected_size=*/2, "");
}

TEST_F(ServerSocketTest, TestClientPacketSizeLower) {
  RunConnect(/*client_max_packet_size=*/3, /*server_max_packet_size=*/4,
             /*expected_size=*/3, "");
}

TEST_F(ServerSocketTest, TestServerPacketSizeLower) {
  RunConnect(/*client_max_packet_size=*/5, /*server_max_packet_size=*/4,
             /*expected_size=*/4, "");
}

TEST_F(ServerSocketTest, TestClientMaxPacketSizeZero) {
  RunConnect(/*client_max_packet_size=*/0, /*server_max_packet_size=*/2,
             /*expected_size=*/2, "");
}

TEST_F(ServerSocketTest, TestConnectInitialData) {
  std::string initial_data("12");
  RunConnect(/*client_max_packet_size=*/2, /*server_max_packet_size=*/2,
             /*expected_size=*/2, initial_data);
  EXPECT_EQ(messages_read_.size(), 1);
  EXPECT_EQ(messages_read_[0], initial_data);
}

TEST_F(ServerSocketTest, TestDisconnect) {
  RunConnect(/*client_max_packet_size=*/2, /*server_max_packet_size=*/2,
             /*expected_size=*/2, "");
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestDisconnectByErrorPacket) {
  expect_connected_ = false;
  RunConnect(/*client_max_packet_size=*/2, /*server_max_packet_size=*/2,
             /*expected_size=*/2, "");
  socket_.OnReceiveControlPacketProxy(Packet::CreateErrorPacket());
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(ServerSocketTest, TestUnexpectedControlPacketConnected) {
  RunConnect(/*client_max_packet_size=*/2, /*server_max_packet_size=*/2,
             /*expected_size=*/2, "");
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(0, 0, 0, "").value());
  EXPECT_THAT(last_error_, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ServerSocketTest, TestUnexpectedControlPacket) {
  expect_connected_ = false;
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(0, 2, "\x56\x78").value());
  EXPECT_THAT(last_error_, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ServerSocketTest, TestBadConnectionRequestLength) {
  expect_connected_ = false;
  auto packet_bytes =
      Packet::CreateConnectionRequestPacket(0, 0, 0, "")->GetBytes();
  // Below the minimum length of 6.
  socket_.OnReceiveControlPacketProxy(
      *Packet::FromBytes(ByteArray(packet_bytes.substr(0, 4))));
  EXPECT_THAT(last_error_, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ServerSocketTest, TestDisconnectNoReconnect) {
  RunConnect(2, 2, 2, "");
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(kProtocolVersion - 1,
                                            kProtocolVersion + 1, 2, "")
          .value());
  EXPECT_TRUE(connection_.NoMorePackets());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestDisconnectTwice) {
  RunConnect(2, 2, 2, "");
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestNoConnectionRequestWrongCommand) {
  expect_connected_ = false;
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(0, 0, "").value());
  EXPECT_THAT(last_error_, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ServerSocketTest, TestUnsupportProtocolVersionTooLow) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(
          kProtocolVersion - 2, kProtocolVersion - 1, kClientMaxPacketSize, "")
          .value());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestUnsupportProtocolVersionTooHigh) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(
          kProtocolVersion + 1, kProtocolVersion + 2, kClientMaxPacketSize, "")
          .value());
  expect_connected_ = false;
}

TEST_F(ServerSocketTest, TestInitiateConnect) {
  expect_connected_ = false;
  socket_.Connect();
}

}  // namespace
}  // namespace weave
}  // namespace nearby
