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

#include "connections/implementation/endpoint_channel_manager.h"

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
#include "absl/time/time.h"
#include "connections/implementation/base_endpoint_channel.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::proto::connections::DisconnectionReason;
using ::location::nearby::proto::connections::Medium;
using EncryptionContext = BaseEndpointChannel::EncryptionContext;

constexpr size_t kChunkSize = 64 * 1024;
constexpr absl::string_view kEndpointId = "EndpointId";
constexpr absl::string_view kMonitorA = "MonitorA";
constexpr absl::string_view kMonitorB = "MonitorB";
constexpr absl::string_view kPumpA = "PumpA";
constexpr absl::string_view kPumpB = "PumpB";

class MockEndpointChannel : public BaseEndpointChannel {
 public:
  explicit MockEndpointChannel(InputStream* input, OutputStream* output)
      : BaseEndpointChannel("service_id", "channel", input, output) {}

  MOCK_METHOD(Medium, GetMedium, (), (const override));
  MOCK_METHOD(void, CloseImpl, (), (override));
};

std::function<void()> MakeDataPump(
    absl::string_view label, InputStream* input, OutputStream* output,
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

std::function<void(const ByteArray&)> MakeDataMonitor(absl::string_view label,
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

std::pair<std::unique_ptr<EncryptionContext>,
          std::unique_ptr<EncryptionContext>>
DoDhKeyExchange(BaseEndpointChannel* channel_a,
                BaseEndpointChannel* channel_b) {
  std::unique_ptr<EncryptionContext> context_a;
  std::unique_ptr<EncryptionContext> context_b;
  EncryptionRunner crypto_a;
  EncryptionRunner crypto_b;
  ClientProxy proxy_a;
  ClientProxy proxy_b;
  CountDownLatch latch(2);
  crypto_a.StartClient(
      &proxy_a, std::string(kEndpointId), channel_a,
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
      &proxy_b, std::string(kEndpointId), channel_b,
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

TEST(BaseEndpointChannelManagerTest, RegisterChannelEncryptedReadwrite) {
  // Setup test communication environment.
  absl::Mutex mutex;
  std::string capture_a;
  std::string capture_b;
  ClientProxy proxy_a;
  ClientProxy proxy_b;
  auto client_a =
      CreatePipe();  // Channel "a" writes to client "a", reads from server "a".
  auto client_b =
      CreatePipe();  // Channel "b" writes to client "b", reads from server "b".
  auto server_a = CreatePipe();  // Data pump "a" reads from client "a", writes
                                 // to server "b".
  auto server_b = CreatePipe();  // Data pump "b" reads from client "b", writes
                                 // to server "a".
  auto channel_a = std::make_unique<MockEndpointChannel>(server_a.first.get(),
                                                         client_a.second.get());
  auto channel_b = std::make_unique<MockEndpointChannel>(server_b.first.get(),
                                                         client_b.second.get());
  auto channel_a_raw = channel_a.get();
  auto channel_b_raw = channel_b.get();

  ON_CALL(*channel_a_raw, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });
  ON_CALL(*channel_b_raw, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });

  MultiThreadExecutor executor(2);
  executor.Execute(
      MakeDataPump(kPumpA, client_a.first.get(), server_b.second.get(),
                   MakeDataMonitor(kMonitorA, &capture_a, &mutex)));
  executor.Execute(
      MakeDataPump(kPumpB, client_b.first.get(), server_a.second.get(),
                   MakeDataMonitor(kMonitorB, &capture_b, &mutex)));

  // Run DH key exchange; setup encryption contexts for channels.
  auto context = DoDhKeyExchange(channel_a.get(), channel_b.get());
  ASSERT_NE(context.first, nullptr);
  ASSERT_NE(context.second, nullptr);

  EndpointChannelManager ecm_a;
  ecm_a.EncryptChannelForEndpoint(std::string(kEndpointId),
                                  std::move(context.first));
  ecm_a.RegisterChannelForEndpoint(&proxy_a, std::string(kEndpointId),
                                   std::move(channel_a));

  EndpointChannelManager ecm_b;
  ecm_b.EncryptChannelForEndpoint(std::string(kEndpointId),
                                  std::move(context.second));
  ecm_b.RegisterChannelForEndpoint(&proxy_b, std::string(kEndpointId),
                                   std::move(channel_b));

  EXPECT_EQ(channel_a_raw->GetType(), "ENCRYPTED_BLUETOOTH");
  EXPECT_EQ(channel_b_raw->GetType(), "ENCRYPTED_BLUETOOTH");

  ByteArray tx_message{"data message"};
  channel_a_raw->Write(tx_message);
  ByteArray rx_message = std::move(channel_b_raw->Read().result());

  // Verify expectations.
  EXPECT_EQ(rx_message, tx_message);
  {
    absl::MutexLock lock(&mutex);
    std::string message{tx_message};
    EXPECT_TRUE(capture_a.find(message) == std::string::npos &&
                capture_b.find(message) == std::string::npos);
  }

  // Shutdown test environment.
  channel_a_raw->Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b_raw->Close(DisconnectionReason::REMOTE_DISCONNECTION);
  ecm_a.UnregisterChannelForEndpoint(
        std::string(kEndpointId), DisconnectionReason::LOCAL_DISCONNECTION,
        ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
  ecm_b.UnregisterChannelForEndpoint(
        std::string(kEndpointId), DisconnectionReason::REMOTE_DISCONNECTION,
        ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
}

TEST(BaseEndpointChannelManagerTest, ReplaceChannelNoEncrypted) {
  // Setup test communication environment.
  absl::Mutex mutex;
  std::string capture_a;
  std::string capture_b;
  ClientProxy proxy_a;
  ClientProxy proxy_b;
  auto client_a =
      CreatePipe();  // Channel "a" writes to client "a", reads from server "a".
  auto client_b =
      CreatePipe();  // Channel "b" writes to client "b", reads from server "b".
  auto server_a = CreatePipe();  // Data pump "a" reads from client "a", writes
                                 // to server "b".
  auto server_b = CreatePipe();  // Data pump "b" reads from client "b", writes
                                 // to server "a".
  auto channel_a = std::make_unique<MockEndpointChannel>(server_a.first.get(),
                                                         client_a.second.get());
  auto channel_b = std::make_unique<MockEndpointChannel>(server_b.first.get(),
                                                         client_b.second.get());
  auto channel_a_raw = channel_a.get();
  auto channel_b_raw = channel_b.get();

  ON_CALL(*channel_a_raw, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });
  ON_CALL(*channel_b_raw, GetMedium).WillByDefault([]() {
    return Medium::BLUETOOTH;
  });

  MultiThreadExecutor executor(2);
  executor.Execute(
      MakeDataPump(kPumpA, client_a.first.get(), server_b.second.get(),
                   MakeDataMonitor(kMonitorA, &capture_a, &mutex)));
  executor.Execute(
      MakeDataPump(kPumpB, client_b.first.get(), server_a.second.get(),
                   MakeDataMonitor(kMonitorB, &capture_b, &mutex)));

  // Run DH key exchange; setup encryption contexts for channels.
  auto context = DoDhKeyExchange(channel_a.get(), channel_b.get());
  ASSERT_NE(context.first, nullptr);
  ASSERT_NE(context.second, nullptr);

  EndpointChannelManager ecm_a;
  ecm_a.EncryptChannelForEndpoint(std::string(kEndpointId),
                                  std::move(context.first));
  ecm_a.ReplaceChannelForEndpoint(&proxy_a, std::string(kEndpointId),
                                  std::move(channel_a), false);

  EndpointChannelManager ecm_b;
  ecm_b.EncryptChannelForEndpoint(std::string(kEndpointId),
                                  std::move(context.second));
  ecm_b.ReplaceChannelForEndpoint(&proxy_b, std::string(kEndpointId),
                                  std::move(channel_b), false);

  EXPECT_EQ(channel_a_raw->GetType(), "BLUETOOTH");
  EXPECT_EQ(channel_b_raw->GetType(), "BLUETOOTH");

  // Shutdown test environment.
  channel_a_raw->Close(DisconnectionReason::LOCAL_DISCONNECTION);
  channel_b_raw->Close(DisconnectionReason::REMOTE_DISCONNECTION);
  ecm_a.UnregisterChannelForEndpoint(
        std::string(kEndpointId), DisconnectionReason::LOCAL_DISCONNECTION,
        ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
  ecm_b.UnregisterChannelForEndpoint(
        std::string(kEndpointId), DisconnectionReason::REMOTE_DISCONNECTION,
        ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);}

}  // namespace
}  // namespace connections
}  // namespace nearby
