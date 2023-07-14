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

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/weave/connection.h"
#include "internal/weave/packet.h"
#include "internal/weave/socket_callback.h"

namespace nearby {
namespace weave {
namespace {

constexpr int kMaxPacketSize = 3;

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
  void OnRemoteTransmitProxy(absl::string_view message) {
    callback_.on_remote_transmit_cb(std::string(message));
  }

 protected:
  int max_packet_size_;
  ConnectionCallback callback_;
  absl::Mutex mutex_;
  std::vector<std::string> packets_written_ ABSL_GUARDED_BY(mutex_);
  bool instant_transmit_ = true;
  bool open_ = false;
};

class FakeSocket : public BaseSocket {
 public:
  explicit FakeSocket(const Connection& connection,
                      SocketCallback&& socketCallback)
      : BaseSocket(connection, std::move(socketCallback)) {}
  ~FakeSocket() override {
    ShutDown();
  }
  MOCK_METHOD(void, Connect, (), (override));
  void OnReceiveControlPacket(Packet packet) override {
    control_packets_.push_back(std::move(packet));
  }
  // Proxies to internal protected methods
  void OnConnectedProxy(int max_packet_size) { OnConnected(max_packet_size); }
  void DisconnectQuietlyProxy() { DisconnectQuietly(); }
  void WriteControlPacketProxy(Packet packet) {
    return WriteControlPacket(std::move(packet));
  }
  void AddControlPacketToQueue(Packet packet) {
    AddControlPacket(std::move(packet));
  }

  std::vector<Packet> control_packets_;
};

Packet CreateDataPacket(int counter, bool first, bool last, ByteArray data) {
  Packet packet = Packet::CreateDataPacket(first, last, data);
  EXPECT_OK(packet.SetPacketCounter(counter));
  return packet;
}

class BaseSocketTest : public ::testing::Test {
 public:
  BaseSocketTest()
      : connection_(FakeConnection(20)),
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
                                     [this](std::string message) {
                                       MutexLock lock(&mutex_);
                                       messages_read_.push_back(message);
                                     },
                                 .on_error_cb =
                                     [this](absl::Status status) {
                                       error_status_ = status;
                                       NEARBY_LOGS(ERROR) << status;
                                     },
                             }) {}
  void TransmitAndFail() {
    socket_.Write(ByteArray("\x01"));
    connection_.OnTransmitProxy(absl::UnavailableError(""));
    absl::SleepFor(absl::Milliseconds(10));
    EXPECT_EQ(connection_.PollWrittenPacket(),
              CreateDataPacket(0, true, true, ByteArray("\x01")).GetBytes());
    Packet err = Packet::CreateErrorPacket();
    EXPECT_OK(err.SetPacketCounter(1));
    EXPECT_EQ(connection_.PollWrittenPacket(), err.GetBytes());
    EXPECT_EQ(error_status_.code(), absl::StatusCode::kUnavailable);
    // clear
    error_status_ = absl::OkStatus();
  }

 protected:
  Mutex mutex_;
  FakeConnection connection_;
  FakeSocket socket_;
  bool connected_ ABSL_GUARDED_BY(mutex_) = true;
  std::vector<std::string> messages_read_;
  absl::Status error_status_;
};

TEST_F(BaseSocketTest, TestConnectQueuedWrite) {
  nearby::Future<absl::Status> result =
      socket_.Write(ByteArray("\x01\x02\x03"));
  EXPECT_FALSE(result.IsSet());
  socket_.OnConnectedProxy(kMaxPacketSize);
  // sleep for 10 ms to allow for status population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_OK(result.Get().GetResult());
  std::string packet = connection_.PollWrittenPacket();
  std::string second = connection_.PollWrittenPacket();
  Packet expected =
      Packet::CreateDataPacket(true, false, ByteArray("\x01\x02"));
  EXPECT_EQ(packet, expected.GetBytes());
  Packet expected2 = Packet::CreateDataPacket(false, true, ByteArray("\x03"));
  EXPECT_OK(expected2.SetPacketCounter(1));
  EXPECT_EQ(second, expected2.GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestDisconnect) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  absl::SleepFor(absl::Milliseconds(10));
  socket_.Disconnect();
  // sleep for 10 ms to allow for executor run to complete
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
  MutexLock lock(&mutex_);
  EXPECT_FALSE(connected_);
}

TEST_F(BaseSocketTest, TestDisconnectTwice) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  socket_.Disconnect();
  absl::SleepFor(absl::Milliseconds(10));
  socket_.Disconnect();
  // sleep for 10 ms to allow for executor run to complete
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
  MutexLock lock(&mutex_);
  EXPECT_FALSE(connected_);
}

TEST_F(BaseSocketTest, DisconnectWithoutConnectDoesNothing) {
  socket_.Disconnect();
  // sleep for 10 ms to allow for executor run to complete
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
  // defaults to true, and the disconnect cb should not have run.
  EXPECT_TRUE(connected_);
}

TEST_F(BaseSocketTest, TestOnDisconnected) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  // sleep for 10 ms to allow for connection status to propagate
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(socket_.IsConnected());
  socket_.DisconnectQuietlyProxy();
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_FALSE(socket_.IsConnected());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteControlPacket) {
  socket_.WriteControlPacketProxy(Packet::CreateErrorPacket());
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  Packet second = Packet::CreateErrorPacket();
  EXPECT_OK(second.SetPacketCounter(1));
  socket_.WriteControlPacketProxy(Packet::CreateErrorPacket());
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(), second.GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteOnePacket) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  nearby::Future<absl::Status> status = socket_.Write(ByteArray("\x01\x02"));
  EXPECT_OK(status.Get().GetResult());
  EXPECT_EQ(
      connection_.PollWrittenPacket(),
      Packet::CreateDataPacket(true, true, ByteArray("\x01\x02")).GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestWriteThreePackets) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  nearby::Future<absl::Status> status =
      socket_.Write(ByteArray("\x01\x02\x03\x04\x05\x06"));
  EXPECT_OK(status.Get().GetResult());
  EXPECT_EQ(
      connection_.PollWrittenPacket(),
      Packet::CreateDataPacket(true, false, ByteArray("\x01\x02")).GetBytes());
  Packet second = Packet::CreateDataPacket(false, false, ByteArray("\x03\x04"));
  EXPECT_OK(second.SetPacketCounter(1));
  EXPECT_EQ(connection_.PollWrittenPacket(), second.GetBytes());
  Packet third = Packet::CreateDataPacket(false, true, ByteArray("\x05\x06"));
  EXPECT_OK(third.SetPacketCounter(2));
  EXPECT_EQ(connection_.PollWrittenPacket(), third.GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestWritePacketCounterRollover) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  for (int i = 0; i <= Packet::kMaxPacketCounter; i++) {
    Packet packet = Packet::CreateDataPacket(true, true, ByteArray("\x01"));
    EXPECT_OK(packet.SetPacketCounter(i));
    nearby::Future<absl::Status> result = socket_.Write(ByteArray("\x01"));
    NEARBY_LOGS(INFO) << "sent packet " << i;
    EXPECT_OK(result.Get().GetResult());
    EXPECT_EQ(connection_.PollWrittenPacket(), packet.GetBytes());
  }
  Packet packet = Packet::CreateDataPacket(true, true, ByteArray("\x01"));
  nearby::Future<absl::Status> result = socket_.Write(ByteArray("\x01"));
  EXPECT_OK(result.Get().GetResult());
  EXPECT_EQ(connection_.PollWrittenPacket(), packet.GetBytes());
}

TEST_F(BaseSocketTest, TestResetByDisconnect) {
  connection_.SetInstantTransmit(false);
  socket_.OnConnectedProxy(kMaxPacketSize);
  nearby::Future<absl::Status> status =
      socket_.Write(ByteArray("\x01\x02\x03"));
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(
      connection_.PollWrittenPacket(),
      Packet::CreateDataPacket(true, false, ByteArray("\x01\x02")).GetBytes());
  ASSERT_TRUE(connection_.NoMorePackets());

  // disconnect should cause resets
  socket_.Disconnect();
  EXPECT_FALSE(status.IsSet());
  EXPECT_TRUE(connection_.NoMorePackets());

  // packet [1, 2] success
  connection_.OnTransmitProxy(absl::OkStatus());
  EXPECT_FALSE(status.IsSet());
  Packet error = Packet::CreateErrorPacket();
  EXPECT_OK(error.SetPacketCounter(1));
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(), error.GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());

  // error packet success
  connection_.OnTransmitProxy(absl::OkStatus());
  ASSERT_TRUE(connection_.NoMorePackets());
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));

  // connect again
  socket_.OnConnectedProxy(kMaxPacketSize);
  NEARBY_LOGS(INFO) << "Reconnected socket";
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(), "");
}

TEST_F(BaseSocketTest, TestResetByControlPacket) {
  connection_.SetInstantTransmit(false);
  socket_.OnConnectedProxy(kMaxPacketSize);
  nearby::Future<absl::Status> status =
      socket_.Write(ByteArray("\x01\x02\x03"));
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(20));
  EXPECT_EQ(connection_.PollWrittenPacket(),
            CreateDataPacket(0, true, false, ByteArray("\x01\x02")).GetBytes());
  socket_.WriteControlPacketProxy(Packet::CreateErrorPacket());
  ASSERT_TRUE(connection_.NoMorePackets());
  // packet [1, 2 success]
  connection_.OnTransmitProxy(absl::OkStatus());
  Packet error = Packet::CreateErrorPacket();
  ASSERT_OK(error.SetPacketCounter(1));
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(), error.GetBytes());
  EXPECT_TRUE(connection_.NoMorePackets());
  // error success
  connection_.OnTransmitProxy(absl::OkStatus());
  // sleep for 10 ms to allow for packet population
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestOnTransmitFailure) {
  connection_.SetInstantTransmit(false);
  socket_.OnConnectedProxy(kMaxPacketSize);
  socket_.Write(ByteArray("\x00"));
  connection_.OnTransmitProxy(absl::InternalError("EOF"));
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_THAT(error_status_,
              testing::status::StatusIs(absl::StatusCode::kInternal));
  MutexLock lock(&mutex_);
  EXPECT_FALSE(connected_);
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitOnePacket) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, true, true, ByteArray("\x01\x02")).GetBytes());
  ASSERT_EQ(1, messages_read_.size());
  EXPECT_EQ(messages_read_[0], "\x01\x02");
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitTwoPackets) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, true, false, ByteArray("\x01\x02")).GetBytes());
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(1, false, true, ByteArray("\x03")).GetBytes());
  ASSERT_EQ(1, messages_read_.size());
  EXPECT_EQ(messages_read_[0], "\x01\x02\x03");
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitThreePackets) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, true, false, ByteArray("\x01\x02")).GetBytes());
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(1, false, false, ByteArray("\x03\x04")).GetBytes());
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(2, false, true, ByteArray("\x05")).GetBytes());
  ASSERT_EQ(1, messages_read_.size());
  EXPECT_EQ(messages_read_[0], "\x01\x02\x03\x04\x05");
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitPacketCounterRollover) {
  for (int i = 0; i <= Packet::kMaxPacketCounter; i++) {
    connection_.OnRemoteTransmitProxy(
        CreateDataPacket(i, true, true, ByteArray("\x01")).GetBytes());
    ASSERT_EQ(1, messages_read_.size());
    EXPECT_EQ(messages_read_[0], "\x01");
    messages_read_.pop_back();
  }
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, true, true, ByteArray("\x01")).GetBytes());
  ASSERT_EQ(1, messages_read_.size());
  EXPECT_EQ(messages_read_[0], "\x01");
  messages_read_.pop_back();
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitIllegalDataPacket) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, false, false, ByteArray("\x00")).GetBytes());
  EXPECT_EQ(error_status_.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitDataPacketWrongPacketCounter) {
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(1, true, false, ByteArray("\x00")).GetBytes());
  EXPECT_EQ(error_status_.code(), absl::StatusCode::kDataLoss);
}

TEST_F(BaseSocketTest, TestOnRemoteTransmitDataPacketOutOfOrderPacketCounter) {
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(0, true, false, ByteArray("\x00")).GetBytes());
  connection_.OnRemoteTransmitProxy(
      CreateDataPacket(2, false, true, ByteArray("\x00")).GetBytes());
  EXPECT_EQ(error_status_.code(), absl::StatusCode::kDataLoss);
}

TEST_F(BaseSocketTest, TestOnRemoteTransitEmpty) {
  connection_.OnRemoteTransmitProxy("");
  EXPECT_EQ(error_status_.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(BaseSocketTest, TestReconnect) {
  connection_.SetInstantTransmit(false);
  socket_.OnConnectedProxy(kMaxPacketSize);
  NEARBY_LOGS(INFO) << "Starting TransmitAndFail1";
  TransmitAndFail();
  NEARBY_LOGS(INFO) << "TransmitAndFail1 completed";
  connection_.OnTransmitProxy(absl::UnavailableError(""));
  EXPECT_EQ(error_status_.code(), absl::StatusCode::kUnavailable);
  absl::SleepFor(absl::Milliseconds(20));
  NEARBY_LOGS(INFO) << "Reconnecting";
  socket_.OnConnectedProxy(kMaxPacketSize);
  absl::SleepFor(absl::Milliseconds(20));
  NEARBY_LOGS(INFO) << "Starting transmit and fail 2";
  TransmitAndFail();
  NEARBY_LOGS(INFO) << "TransmitAndFail2 completed";
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestControlGoesBeforeMessage) {
  connection_.SetInstantTransmit(false);
  socket_.OnConnectedProxy(kMaxPacketSize);
  absl::SleepFor(absl::Milliseconds(10));
  socket_.AddControlPacketToQueue(Packet::CreateErrorPacket());
  auto future = socket_.Write(ByteArray("\x01"));
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_EQ(connection_.PollWrittenPacket(),
            Packet::CreateErrorPacket().GetBytes());
  EXPECT_FALSE(future.IsSet());
  EXPECT_TRUE(connection_.NoMorePackets());
  connection_.OnTransmitProxy(absl::OkStatus());
  absl::SleepFor(absl::Milliseconds(20));
  EXPECT_FALSE(future.IsSet());
  EXPECT_TRUE(connection_.NoMorePackets());
}

TEST_F(BaseSocketTest, TestDisconnectOnBadDataPacketCounter) {
  socket_.OnConnectedProxy(kMaxPacketSize);
  absl::SleepFor(absl::Milliseconds(10));
  Packet bad_packet = CreateDataPacket(2, true, true, ByteArray("\x01"));
  connection_.OnRemoteTransmitProxy(bad_packet.GetBytes());
  absl::SleepFor(absl::Milliseconds(10));
  EXPECT_FALSE(socket_.IsConnected());
  MutexLock lock(&mutex_);
  EXPECT_FALSE(connected_);
}

}  // namespace
}  // namespace weave
}  // namespace nearby
