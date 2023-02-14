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

// NOLINTNEXTLINE
#include <future>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/socket_abstraction/connection.h"
#include "internal/socket_abstraction/v0/client_socket.h"
#include "internal/socket_abstraction/v0/server_socket.h"

namespace nearby {
namespace socket_abstraction {
namespace {

constexpr int kClientMaxPacketSize = 4;
constexpr int kServerMaxPacketSize = 3;
constexpr int64_t kLatencyMs = 10;
constexpr absl::string_view kMessage = "Hello world";

class PipedConnection : public Connection {
 public:
  PipedConnection(absl::string_view name, int max_packet_size) {
    name_ = name;
    max_packet_size_ = max_packet_size;
  }

  ~PipedConnection() override {
    CountDownLatch latch(1);
    executor_.Execute("Cleaner", [&latch]() { latch.CountDown(); });
    latch.Await();
    executor_.Shutdown();
  }

  void Connect(PipedConnection* remote) { remote_ = remote; }

  void Initialize(ConnectionCallback cb) override { cb_ = cb; }

  int GetMaxPacketSize() override { return max_packet_size_; }

  void Transmit(ByteArray packet) override {
    if (!open_) {
      executor_.Execute("OnTransmit", [this]() {
        cb_.on_transmit_cb(absl::UnavailableError("Connection not open"));
      });
      return;
    }
    NEARBY_LOGS(INFO) << name_ << " transmit " << packet.AsStringView();
    absl::SleepFor(absl::Milliseconds(kLatencyMs));
    executor_.Execute("OnTransmit",
                      [this]() { cb_.on_transmit_cb(absl::OkStatus()); });
    absl::SleepFor(absl::Milliseconds(kLatencyMs));
    executor_.Execute("OnRemoteTransmit", [packet, this]() {
      remote_->cb_.on_remote_transmit_cb(packet);
    });
  }

  void Close() override {
    if (open_) {
      open_ = false;
      remote_->Close();
    }
  }

 private:
  std::string name_;
  int max_packet_size_;
  SingleThreadExecutor executor_;
  PipedConnection* remote_;
  ConnectionCallback cb_;
  bool open_ = true;
};

class ClientFakeSocket : public ClientSocket {
 public:
  explicit ClientFakeSocket(Connection* connection)
      : ClientSocket(
            connection,
            SocketCallback{
                .on_connected_cb =
                    [this]() {
                      //  MutexLock lock(&mutex_);
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
                      NEARBY_LOGS(INFO) << "Client OnReceiveCb message: "
                                        << message.AsStringView();
                      messages_read_.push_back(message);
                    },
                .on_error_cb =
                    [](absl::Status status) { NEARBY_LOGS(ERROR) << status; },
            }) {}
  bool IsConnected() override { return connected_; }
  std::vector<ByteArray> messages_read_;

 private:
  Mutex mutex_;
  bool connected_;
};

class ServerFakeSocket : public ServerSocket {
 public:
  explicit ServerFakeSocket(Connection* connection)
      : ServerSocket(
            connection,
            SocketCallback{
                .on_connected_cb =
                    [this]() {
                      //  MutexLock lock(&mutex_);
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
                      NEARBY_LOGS(INFO) << "Server OnReceiveCb message: "
                                        << message.AsStringView();
                      messages_read_.push_back(message);
                    },
                .on_error_cb =
                    [](absl::Status status) { NEARBY_LOGS(ERROR) << status; },
            }) {}
  bool IsConnected() override { return connected_; }
  std::vector<ByteArray> messages_read_;

 private:
  Mutex mutex_;
  bool connected_;
};

class ClientServerSocketTest : public ::testing::Test {
 public:
  ClientServerSocketTest()
      : client_connection_(PipedConnection("Client", kClientMaxPacketSize)),
        server_connection_(PipedConnection("Server", kServerMaxPacketSize)),
        client_(ClientFakeSocket(&client_connection_)),
        server_(ServerFakeSocket(&server_connection_)) {}

  void SetUp() override {
    client_connection_.Connect(&server_connection_);
    server_connection_.Connect(&client_connection_);
  }
  PipedConnection client_connection_;
  PipedConnection server_connection_;
  ClientFakeSocket client_;
  ServerFakeSocket server_;
};

TEST_F(ClientServerSocketTest, ClientToServer) {
  client_.Connect();
  EXPECT_TRUE(client_.IsConnected());
  EXPECT_TRUE(server_.IsConnected());
  std::future<absl::Status> status =
      client_.Write(ByteArray(std::string(kMessage)));
  EXPECT_OK(status.get());
  ASSERT_EQ(server_.messages_read_.size(), 1);
  EXPECT_EQ(server_.messages_read_[0].AsStringView(), kMessage);
}

TEST_F(ClientServerSocketTest, ServerToClient) {
  client_.Connect();
  EXPECT_TRUE(client_.IsConnected());
  EXPECT_TRUE(server_.IsConnected());
  std::future<absl::Status> status =
      server_.Write(ByteArray(std::string(kMessage)));
  EXPECT_OK(status.get());
  absl::SleepFor(absl::Milliseconds(10));
  ASSERT_EQ(client_.messages_read_.size(), 1);
  EXPECT_EQ(client_.messages_read_[0].AsStringView(), kMessage);
}

TEST_F(ClientServerSocketTest, TestConnect) {
  client_.Connect();
  EXPECT_TRUE(client_.IsConnected());
  EXPECT_TRUE(server_.IsConnected());
}

}  // namespace
}  // namespace socket_abstraction
}  // namespace nearby
