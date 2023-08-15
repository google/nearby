// Copyright 2020 Google LLC
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

#include "connections/implementation/base_endpoint_channel.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "securegcm/ukey2_handshake.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::proto::connections::DisconnectionReason;
using ::location::nearby::proto::connections::Medium;
using EncryptionContext = BaseEndpointChannel::EncryptionContext;
constexpr size_t kChunkSize = 64 * 1024;

class TestEndpointChannel : public BaseEndpointChannel {
 public:
  explicit TestEndpointChannel(InputStream* input, OutputStream* output)
      : BaseEndpointChannel("service_id", "channel", input, output) {}

  using BaseEndpointChannel::EncodeMessageForTests;

  MOCK_METHOD(Medium, GetMedium, (), (const override));
  MOCK_METHOD(void, CloseImpl, (), (override));
};

std::function<void()> MakeDataPump(
    std::string label, InputStream* input, OutputStream* output,
    std::function<void(const ByteArray&)> monitor = nullptr) {
  return [label, input, output, monitor]() {
    NEARBY_LOGS(INFO) << "streaming data through '" << label << "'";
    while (true) {
      auto read_response = input->Read(kChunkSize);
      if (!read_response.ok()) {
        NEARBY_LOGS(INFO) << "Peer reader closed on '" << label << "'";
        output->Close();
        break;
      }
      if (monitor) {
        monitor(read_response.result());
      }
      auto write_response = output->Write(read_response.result());
      if (write_response.Raised()) {
        NEARBY_LOGS(INFO) << "Peer writer closed on '" << label << "'";
        input->Close();
        break;
      }
    }
    NEARBY_LOGS(INFO) << "streaming terminated on '" << label << "'";
  };
}

std::function<void(const ByteArray&)> MakeDataMonitor(const std::string& label,
                                                      std::string* capture,
                                                      absl::Mutex* mutex) {
  return [label, capture, mutex](const ByteArray& input) mutable {
    std::string s = std::string(input);
    {
      absl::MutexLock lock(mutex);
      *capture += s;
    }
    NEARBY_LOGS(INFO) << "source='" << label << "'"
                      << "; message='" << s << "'";
  };
}

std::pair<std::shared_ptr<EncryptionContext>,
          std::shared_ptr<EncryptionContext>>
DoDhKeyExchange(BaseEndpointChannel* channel_a,
                BaseEndpointChannel* channel_b) {
  std::shared_ptr<EncryptionContext> context_a;
  std::shared_ptr<EncryptionContext> context_b;
  EncryptionRunner crypto_a;
  EncryptionRunner crypto_b;
  ClientProxy proxy_a;
  ClientProxy proxy_b;
  CountDownLatch latch(2);
  crypto_a.StartClient(
      &proxy_a, "endpoint_id", channel_a,
      {
          .on_success_cb =
              [&latch, &context_a](
                  const std::string& endpoint_id,
                  std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                  const std::string& auth_token,
                  const ByteArray& raw_auth_token) {
                NEARBY_LOGS(INFO) << "client-A side key negotiation done";
                EXPECT_TRUE(ukey2->VerifyHandshake());
                auto context = ukey2->ToConnectionContext();
                EXPECT_NE(context, nullptr);
                context_a = std::move(context);
                latch.CountDown();
              },
          .on_failure_cb =
              [&latch](const std::string& endpoint_id,
                       EndpointChannel* channel) {
                NEARBY_LOGS(INFO) << "client-A side key negotiation failed";
                latch.CountDown();
              },
      });
  crypto_b.StartServer(
      &proxy_b, "endpoint_id", channel_b,
      {
          .on_success_cb =
              [&latch, &context_b](
                  const std::string& endpoint_id,
                  std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                  const std::string& auth_token,
                  const ByteArray& raw_auth_token) {
                NEARBY_LOGS(INFO) << "client-B side key negotiation done";
                EXPECT_TRUE(ukey2->VerifyHandshake());
                auto context = ukey2->ToConnectionContext();
                EXPECT_NE(context, nullptr);
                context_b = std::move(context);
                latch.CountDown();
              },
          .on_failure_cb =
              [&latch](const std::string& endpoint_id,
                       EndpointChannel* channel) {
                NEARBY_LOGS(INFO) << "client-B side key negotiation failed";
                latch.CountDown();
              },
      });
  EXPECT_TRUE(latch.Await(absl::Milliseconds(5000)).result());
  return std::make_pair(std::move(context_a), std::move(context_b));
}

TEST(BaseEndpointChannelTest, ConstructorDestructorWorks) {
  auto [input, output] = CreatePipe();

  TestEndpointChannel test_channel(input.get(), output.get());
}

TEST(BaseEndpointChannelTest, ReadWrite) {
  // Direct not-encrypted IO.
  auto pipe_a = CreatePipe();  // channel_a writes to pipe_a, reads from pipe_b.
  auto pipe_b = CreatePipe();  // channel_b writes to pipe_b, reads from pipe_a.
  TestEndpointChannel channel_a(pipe_b.first.get(), pipe_a.second.get());
  TestEndpointChannel channel_b(pipe_a.first.get(), pipe_b.second.get());
  ByteArray tx_message{"data message"};
  channel_a.Write(tx_message);
  ByteArray rx_message = std::move(channel_b.Read().result());
  EXPECT_EQ(rx_message, tx_message);
}

TEST(BaseEndpointChannelTest, ChannelUnencryptedByDefault) {
  auto pipe = CreatePipe();
  TestEndpointChannel channel(pipe.first.get(), pipe.second.get());

  ExceptionOr<ByteArray> result = channel.TryDecrypt(ByteArray("message"));

  EXPECT_FALSE(channel.IsEncrypted());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kFailed);
}

TEST(BaseEndpointChannelTest, TryDecrypt) {
  absl::string_view kMessage = "message";
  auto pipe_a = CreatePipe();  // channel_a writes to pipe_a, reads from pipe_b.
  auto pipe_b = CreatePipe();  // channel_b writes to pipe_b, reads from pipe_a.
  TestEndpointChannel channel_a(pipe_b.first.get(), pipe_a.second.get());
  TestEndpointChannel channel_b(pipe_a.first.get(), pipe_b.second.get());
  auto [context_a, context_b] = DoDhKeyExchange(&channel_a, &channel_b);
  ASSERT_NE(context_a, nullptr);
  ASSERT_NE(context_b, nullptr);
  channel_a.EnableEncryption(context_a);
  channel_b.EnableEncryption(context_b);
  std::unique_ptr<std::string> encrypted_message =
      channel_a.EncodeMessageForTests(kMessage);

  ExceptionOr<ByteArray> decrypted_message =
      channel_b.TryDecrypt(ByteArray(*encrypted_message));

  EXPECT_TRUE(channel_b.IsEncrypted());
  EXPECT_TRUE(decrypted_message.ok());
  EXPECT_EQ(decrypted_message.result().AsStringView(), kMessage);
}

TEST(BaseEndpointChannelTest, TryDecryptFailsWhenDecryptionFails) {
  auto pipe_a = CreatePipe();  // channel_a writes to pipe_a, reads from pipe_b.
  auto pipe_b = CreatePipe();  // channel_b writes to pipe_b, reads from pipe_a.
  TestEndpointChannel channel_a(pipe_b.first.get(), pipe_a.second.get());
  TestEndpointChannel channel_b(pipe_a.first.get(), pipe_b.second.get());
  auto [context_a, context_b] = DoDhKeyExchange(&channel_a, &channel_b);
  ASSERT_NE(context_a, nullptr);
  channel_a.EnableEncryption(context_a);

  ExceptionOr<ByteArray> result =
      channel_a.TryDecrypt(ByteArray("invalid message"));

  EXPECT_TRUE(channel_a.IsEncrypted());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kExecution);
}

TEST(BaseEndpointChannelTest, NotEncryptedReadWriteCanBeIntercepted) {
  // Not encrypted IO; MITM scenario.

  // Setup test communication environment.
  absl::Mutex mutex;
  std::string capture_a;
  std::string capture_b;
  auto client_a =
      CreatePipe();  // Channel "a" writes to client "a", reads from server "a".
  auto client_b =
      CreatePipe();  // Channel "b" writes to client "b", reads from server "b".
  auto server_a = CreatePipe();  // Data pump "a" reads from client "a", writes
                                 // to server "b".
  auto server_b = CreatePipe();  // Data pump "b" reads from client "b", writes
                                 // to server "a".
  TestEndpointChannel channel_a(server_a.first.get(), client_a.second.get());
  TestEndpointChannel channel_b(server_b.first.get(), client_b.second.get());

  ON_CALL(channel_a, GetMedium).WillByDefault([]() { return Medium::BLE; });
  ON_CALL(channel_b, GetMedium).WillByDefault([]() { return Medium::BLE; });

  MultiThreadExecutor executor(2);
  executor.Execute(
      MakeDataPump("pump_a", client_a.first.get(), server_b.second.get(),
                   MakeDataMonitor("monitor_a", &capture_a, &mutex)));
  executor.Execute(
      MakeDataPump("pump_b", client_b.first.get(), server_a.second.get(),
                   MakeDataMonitor("monitor_b", &capture_b, &mutex)));

  EXPECT_EQ(channel_a.GetType(), "BLE");
  EXPECT_EQ(channel_b.GetType(), "BLE");

  // Start data transfer
  ByteArray tx_message{"data message"};
  channel_a.Write(tx_message);
  ByteArray rx_message = std::move(channel_b.Read().result());

  // Verify expectations.
  EXPECT_EQ(rx_message, tx_message);
  {
    absl::MutexLock lock(&mutex);
    std::string message{tx_message};
    EXPECT_TRUE(capture_a.find(message) != std::string::npos ||
                capture_b.find(message) != std::string::npos);
  }

  // Shutdown test environment.
  channel_a.Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b.Close(DisconnectionReason::REMOTE_DISCONNECTION);
}

TEST(BaseEndpointChannelTest, EncryptedReadWriteCanNotBeIntercepted) {
  // Encrypted IO; MITM scenario.

  // Setup test communication environment.
  absl::Mutex mutex;
  std::string capture_a;
  std::string capture_b;
  auto client_a =
      CreatePipe();  // Channel "a" writes to client "a", reads from server "a".
  auto client_b =
      CreatePipe();  // Channel "b" writes to client "b", reads from server "b".
  auto server_a = CreatePipe();  // Data pump "a" reads from client "a", writes
                                 // to server "b".
  auto server_b = CreatePipe();  // Data pump "b" reads from client "b", writes
                                 // to server "a".
  TestEndpointChannel channel_a(server_a.first.get(), client_a.second.get());
  TestEndpointChannel channel_b(server_b.first.get(), client_b.second.get());

  ON_CALL(channel_a, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });
  ON_CALL(channel_b, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });

  MultiThreadExecutor executor(2);
  executor.Execute(
      MakeDataPump("pump_a", client_a.first.get(), server_b.second.get(),
                   MakeDataMonitor("monitor_a", &capture_a, &mutex)));
  executor.Execute(
      MakeDataPump("pump_b", client_b.first.get(), server_a.second.get(),
                   MakeDataMonitor("monitor_b", &capture_b, &mutex)));

  // Run DH key exchange; setup encryption contexts for channels.
  auto [context_a, context_b] = DoDhKeyExchange(&channel_a, &channel_b);
  ASSERT_NE(context_a, nullptr);
  ASSERT_NE(context_b, nullptr);
  channel_a.EnableEncryption(context_a);
  channel_b.EnableEncryption(context_b);

  EXPECT_EQ(channel_a.GetType(), "ENCRYPTED_BLUETOOTH");
  EXPECT_EQ(channel_b.GetType(), "ENCRYPTED_BLUETOOTH");
  EXPECT_TRUE(channel_a.IsEncrypted());
  EXPECT_TRUE(channel_b.IsEncrypted());

  // Start data transfer
  ByteArray tx_message{"data message"};
  channel_a.Write(tx_message);
  ByteArray rx_message = std::move(channel_b.Read().result());

  // Verify expectations.
  EXPECT_EQ(rx_message, tx_message);
  {
    absl::MutexLock lock(&mutex);
    std::string message{tx_message};
    EXPECT_TRUE(capture_a.find(message) == std::string::npos &&
                capture_b.find(message) == std::string::npos);
  }

  // Shutdown test environment.
  channel_a.Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b.Close(DisconnectionReason::REMOTE_DISCONNECTION);
}

TEST(BaseEndpointChannelTest, CanBesuspendedAndResumed) {
  // Setup test communication environment.
  auto pipe_a = CreatePipe();  // channel_a writes to pipe_a, reads from pipe_b.
  auto pipe_b = CreatePipe();  // channel_b writes to pipe_b, reads from pipe_a.
  TestEndpointChannel channel_a(pipe_b.first.get(), pipe_a.second.get());
  TestEndpointChannel channel_b(pipe_a.first.get(), pipe_b.second.get());

  ON_CALL(channel_a, GetMedium).WillByDefault([]() {
    return Medium::WIFI_LAN;
  });
  ON_CALL(channel_b, GetMedium).WillByDefault([]() {
    return Medium::WIFI_LAN;
  });

  EXPECT_EQ(channel_a.GetType(), "WIFI_LAN");
  EXPECT_EQ(channel_b.GetType(), "WIFI_LAN");

  // Start data transfer
  ByteArray tx_message{"data message"};
  ByteArray more_message{"more data"};
  channel_a.Write(tx_message);
  ByteArray rx_message = std::move(channel_b.Read().result());

  // Pause and make sure reader blocks.
  MultiThreadExecutor pause_resume_executor(2);
  channel_a.Pause();
  pause_resume_executor.Execute([&channel_a, &more_message]() {
    // Write will block until channel is resumed, or closed.
    EXPECT_TRUE(channel_a.Write(more_message).Ok());
  });
  CountDownLatch latch(1);
  ByteArray read_more;
  pause_resume_executor.Execute([&channel_b, &read_more, &latch]() {
    // Read will block until channel is resumed, or closed.
    auto response = channel_b.Read();
    EXPECT_TRUE(response.ok());
    read_more = std::move(response.result());
    latch.CountDown();
  });
  absl::SleepFor(absl::Milliseconds(500));
  EXPECT_TRUE(read_more.Empty());

  // Resume; verify that data transfer comepleted.
  channel_a.Resume();
  EXPECT_TRUE(latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(read_more, more_message);

  // Shutdown test environment.
  channel_a.Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b.Close(DisconnectionReason::REMOTE_DISCONNECTION);
}

TEST(BaseEndpointChannelTest, ReadAfterInputStreamClosed) {
  auto [input, output] = CreatePipe();

  TestEndpointChannel test_channel(input.get(), output.get());

  // Close the output stream before trying to read from the input.
  output->Close();

  // Trying to read should fail gracefully with an IO error.
  ExceptionOr<ByteArray> read_data = test_channel.Read();

  ASSERT_FALSE(read_data.ok());
  ASSERT_TRUE(read_data.GetException().Raised(Exception::kIo));
}

TEST(BaseEndpointChannelTest, ReadUnencryptedFrameOnEncryptedChannel) {
  // Setup test communication environment.
  auto pipe_a = CreatePipe();  // channel_a writes to pipe_a, reads from pipe_b.
  auto pipe_b = CreatePipe();  // channel_b writes to pipe_b, reads from pipe_a.
  TestEndpointChannel channel_a(pipe_b.first.get(), pipe_a.second.get());
  TestEndpointChannel channel_b(pipe_a.first.get(), pipe_b.second.get());

  ON_CALL(channel_a, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });
  ON_CALL(channel_b, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });

  // Run DH key exchange; setup encryption contexts for channels. But only
  // encrypt |channel_b|.
  auto [context_a, context_b] = DoDhKeyExchange(&channel_a, &channel_b);
  ASSERT_NE(context_a, nullptr);
  ASSERT_NE(context_b, nullptr);
  channel_b.EnableEncryption(context_b);

  EXPECT_EQ(channel_a.GetType(), "BLUETOOTH");
  EXPECT_EQ(channel_b.GetType(), "ENCRYPTED_BLUETOOTH");

  // An unencrypted KeepAlive should succeed.
  ByteArray keep_alive_message = parser::ForKeepAlive();
  channel_a.Write(keep_alive_message);
  ExceptionOr<ByteArray> result = channel_b.Read();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), keep_alive_message);

  // An unencrypted data frame should fail.
  ByteArray tx_message{"data message"};
  channel_a.Write(tx_message);
  result = channel_b.Read();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kInvalidProtocolBuffer);

  // Shutdown test environment.
  channel_a.Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b.Close(DisconnectionReason::REMOTE_DISCONNECTION);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
