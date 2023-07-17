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

#include "internal/weave/sockets/client_socket.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/weave/sockets/initial_data_provider.h"

namespace nearby {
namespace weave {
namespace {

constexpr int kClientMaxPacketSize = 4;
constexpr int kServerMaxPacketSize = 3;
constexpr int kProtocolVersion = 1;

class TestClientSocket : public ClientSocket {
 public:
  TestClientSocket(const Connection& connection, SocketCallback&& callback)
      : ClientSocket(connection, std::move(callback)) {}
  TestClientSocket(const Connection& connection, SocketCallback&& callback,
                   std::unique_ptr<InitialDataProvider> provider)
      : ClientSocket(connection, std::move(callback), std::move(provider)) {}
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

class ClientSocketTest : public ::testing::Test {
 public:
  ClientSocketTest()
      : connection_(FakeConnection(kClientMaxPacketSize)),
        socket_(TestClientSocket(connection_,
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
    EXPECT_EQ(socket_.IsConnected(), expect_connected_);
  }
  void RunConnect(int client_size, int server_size,
                  absl::string_view initial_data) {
    connection_.SetMaxPacketSize(client_size);
    NEARBY_LOGS(INFO) << "connect";
    socket_.Connect();
    absl::SleepFor(absl::Milliseconds(10));
    auto packet = Packet::FromBytes(ByteArray(connection_.PollWrittenPacket()));
    ASSERT_OK(packet);
    EXPECT_TRUE(packet->IsControlPacket());
    EXPECT_EQ(packet->GetControlCommandNumber(),
              Packet::ControlPacketType::kControlConnectionRequest);
    EXPECT_EQ(packet->GetPacketCounter(), 0);
    int16_t min_protocol_version = ((packet->GetPayload().data()[0] << 8) |
                                    packet->GetPayload().data()[1]) &
                                   (int16_t)0xFFFF;
    int16_t max_protocol_version = ((packet->GetPayload().data()[2] << 8) |
                                    packet->GetPayload().data()[3]) &
                                   (int16_t)0xFFFF;
    EXPECT_EQ(min_protocol_version, kProtocolVersion);
    EXPECT_EQ(max_protocol_version, kProtocolVersion);
    int pkt_size = ((packet->GetPayload().data()[4] << 8) |
                    packet->GetPayload().data()[5]) &
                   (int16_t)0xFFFF;
    EXPECT_EQ(pkt_size, client_size);
    EXPECT_EQ(packet->GetPayload().size(), 6);
    socket_.OnReceiveControlPacketProxy(
        Packet::CreateConnectionConfirmPacket(kProtocolVersion, server_size,
                                              initial_data)
            .value());
    int expected_payload_size = server_size - 1;
    auto status = socket_.Write(ByteArray(expected_payload_size));
    EXPECT_OK(status.Get().GetResult());
    absl::SleepFor(absl::Milliseconds(10));
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
    status = socket_.Write(ByteArray(expected_payload_size + 1));
    absl::SleepFor(absl::Milliseconds(10));
    EXPECT_OK(status.Get().GetResult());
    ASSERT_FALSE(connection_.NoMorePackets());
    connection_.PollWrittenPacket();
    connection_.PollWrittenPacket();
    EXPECT_TRUE(connection_.NoMorePackets());
  }
  FakeConnection connection_;
  TestClientSocket socket_;
  Mutex mutex_;
  bool connected_ = true;
  std::vector<std::string> messages_read_;
  bool expect_connected_ = true;
  absl::Status last_error_;
};

TEST_F(ClientSocketTest, TestClientServerPacketSizeSame) {
  RunConnect(2, 2, "");
}

TEST_F(ClientSocketTest, TestClientServerPacketSizeLower) {
  RunConnect(kClientMaxPacketSize, kServerMaxPacketSize, "");
}

TEST_F(ClientSocketTest, TestClientServerPacketSizeHigher) {
  int client_size = 4;
  int server_size = 5;
  connection_.SetMaxPacketSize(client_size);
  socket_.Connect();
  absl::SleepFor(absl::Milliseconds(10));
  auto packet = Packet::FromBytes(ByteArray(connection_.PollWrittenPacket()));
  ASSERT_OK(packet);
  EXPECT_TRUE(packet->IsControlPacket());
  EXPECT_EQ(packet->GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionRequest);
  EXPECT_EQ(packet->GetPacketCounter(), 0);
  int16_t min_protocol_version =
      ((packet->GetPayload().data()[0] << 8) | packet->GetPayload().data()[1]) &
      (int16_t)0xFFFF;
  int16_t max_protocol_version =
      ((packet->GetPayload().data()[2] << 8) | packet->GetPayload().data()[3]) &
      (int16_t)0xFFFF;
  EXPECT_EQ(min_protocol_version, kProtocolVersion);
  EXPECT_EQ(max_protocol_version, kProtocolVersion);
  int pkt_size =
      ((packet->GetPayload().data()[4] << 8) | packet->GetPayload().data()[5]) &
      (int16_t)0xFFFF;
  EXPECT_EQ(pkt_size, client_size);
  EXPECT_EQ(packet->GetPayload().size(), 6);
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(kProtocolVersion, server_size, "")
          .value());
  absl::SleepFor(absl::Milliseconds(10));
  expect_connected_ = false;
}

TEST_F(ClientSocketTest, TestConnectInitialData) {
  std::string initial_data = "12";
  RunConnect(2, 2, initial_data);
  EXPECT_EQ(messages_read_.size(), 1);
  EXPECT_EQ(messages_read_[0], initial_data);
}

TEST_F(ClientSocketTest, TestConnectConnect) {
  RunConnect(2, 2, "");
  ASSERT_TRUE(connection_.NoMorePackets());
  socket_.Connect();
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(ClientSocketTest, TestResponseBeforeOnTransmit) {
  connection_.SetInstantTransmit(false);
  socket_.Connect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  auto packet = Packet::FromBytes(ByteArray(connection_.PollWrittenPacket()));
  EXPECT_TRUE(packet->IsControlPacket());
  EXPECT_EQ(packet->GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionRequest);
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(kProtocolVersion, 2, "").value());
  absl::SleepFor(absl::Milliseconds(10));
  connection_.OnTransmitProxy(absl::OkStatus());
}

TEST_F(ClientSocketTest, TestDisconnect) {
  RunConnect(2, 2, "");
  ASSERT_TRUE(connection_.NoMorePackets());
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  expect_connected_ = false;
}

TEST_F(ClientSocketTest, TestDisconnectConnect) {
  RunConnect(2, 2, "");
  ASSERT_TRUE(connection_.NoMorePackets());
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_FALSE(connection_.NoMorePackets());
  Packet expected = Packet::CreateErrorPacket();
  EXPECT_OK(expected.SetPacketCounter(4));
  EXPECT_EQ(connection_.PollWrittenPacket(), expected.GetBytes());
  RunConnect(2, 2, "");
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_TRUE(connection_.NoMorePackets());
  expect_connected_ = true;
}

TEST_F(ClientSocketTest, TestReceiveErrorPacket) {
  RunConnect(2, 2, "");
  ASSERT_TRUE(connection_.NoMorePackets());
  socket_.OnReceiveControlPacketProxy(Packet::CreateErrorPacket());
  absl::SleepFor(absl::Milliseconds(10));
  expect_connected_ = false;
}

TEST_F(ClientSocketTest, TestNoConnectionConfirmWrongCommandNumber) {
  socket_.Connect();
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(0, 0, 0, "").value());
  expect_connected_ = false;
}

TEST_F(ClientSocketTest, TestUnexpectedControlPacketNoConnection) {
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(0, 0, 0, "").value());
  expect_connected_ = false;
  EXPECT_EQ(last_error_.code(), absl::StatusCode::kInternal);
}

TEST_F(ClientSocketTest, TestUnexpectedControlPacketConnected) {
  RunConnect(2, 2, "");
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionRequestPacket(0, 0, 0, "").value());
  EXPECT_EQ(last_error_.code(), absl::StatusCode::kInternal);
}

TEST_F(ClientSocketTest, TestBadConnectionConfirmPacket) {
  socket_.Connect();
  auto packet =
      Packet::CreateConnectionConfirmPacket(kProtocolVersion, 6, "").value();
  socket_.OnReceiveControlPacketProxy(
      *Packet::FromBytes(ByteArray(packet.GetBytes().substr(0, 2))));
  expect_connected_ = false;
  EXPECT_EQ(last_error_.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ClientSocketTest, TestUnsupportedProtocolVersion) {
  socket_.Connect();
  socket_.OnReceiveControlPacketProxy(
      Packet::CreateConnectionConfirmPacket(kProtocolVersion + 1,
                                            kServerMaxPacketSize, "")
          .value());
  expect_connected_ = false;
}

TEST_F(ClientSocketTest, TestSocketWithRandomDataProvider) {
  auto provider = std::make_unique<RandomDataProvider>(/*number_of_bytes=*/5);
  TestClientSocket socket(
      connection_,
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
              [](absl::Status status) { NEARBY_LOGS(ERROR) << status; },
      },
      std::move(provider));
  socket.Connect();
  absl::SleepFor(absl::Milliseconds(10));
  Packet packet =
      *Packet::FromBytes(ByteArray(connection_.PollWrittenPacket()));
  EXPECT_TRUE(packet.IsControlPacket());
  EXPECT_EQ(packet.GetControlCommandNumber(),
            Packet::ControlPacketType::kControlConnectionRequest);
  // 2 + 2 + 2 + 5.
  EXPECT_EQ(packet.GetPayload().size(), 11);
  EXPECT_NE(packet.GetPayload().substr(5), "\x00\x00\x00\x00\x00");
  // we never sent back a connection confirm so we are still disconnected.
  expect_connected_ = false;
}

}  // namespace
}  // namespace weave
}  // namespace nearby
