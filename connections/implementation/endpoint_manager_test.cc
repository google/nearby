// Copyright 2021 Google LLC
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

#include "connections/implementation/endpoint_manager.h"

#include <atomic>
#include <cstdint>
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
#include "connections/connection_options.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/offline_frames.h"
#include "connections/listeners.h"
#include "connections/status.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/test/fake_single_thread_executor.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::PayloadTransferFrame;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::DisconnectionReason;
using ::location::nearby::proto::connections::Medium;
using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

class MockEndpointChannel : public EndpointChannel {
 public:
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (), (override));
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (PacketMetaData & packet_meta_data),
              (override));
  MOCK_METHOD(Exception, Write, (const ByteArray& data), (override));
  MOCK_METHOD(Exception, Write,
              (const ByteArray& data, PacketMetaData& packet_meta_data),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, Close, (DisconnectionReason reason), (override));
  MOCK_METHOD(void, Close,
              (DisconnectionReason reason,
               location::nearby::analytics::proto::ConnectionsLog::
                   EstablishedConnection::SafeDisconnectionResult result),
              (override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionTechnology,
              GetTechnology, (), (const, override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionBand, GetBand, (),
              (const, override));
  MOCK_METHOD(int, GetFrequency, (), (const, override));
  MOCK_METHOD(int, GetTryCount, (), (const, override));
  MOCK_METHOD(std::string, GetType, (), (const, override));
  MOCK_METHOD(std::string, GetServiceId, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(Medium, GetMedium, (), (const, override));
  MOCK_METHOD(int, GetMaxTransmitPacketSize, (), (const, override));
  MOCK_METHOD(void, EnableEncryption,
              (std::shared_ptr<EncryptionContext> context), (override));
  MOCK_METHOD(void, DisableEncryption, (), (override));
  MOCK_METHOD(bool, IsPaused, (), (const, override));
  MOCK_METHOD(bool, IsEncrypted, (), (override));
  MOCK_METHOD(ExceptionOr<ByteArray>, TryDecrypt, (const ByteArray& data),
              (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(absl::Time, GetLastReadTimestamp, (), (const, override));
  MOCK_METHOD(absl::Time, GetLastWriteTimestamp, (), (const, override));
  MOCK_METHOD(void, SetAnalyticsRecorder,
              (analytics::AnalyticsRecorder*, const std::string&), (override));

  bool IsClosed() const override {
    absl::MutexLock lock(&mutex_);
    return closed_;
  }
  void DoClose() {
    absl::MutexLock lock(&mutex_);
    closed_ = true;
  }

 private:
  mutable absl::Mutex mutex_;
  bool closed_ = false;
};

class MockFrameProcessor : public EndpointManager::FrameProcessor {
 public:
  MOCK_METHOD(void, OnIncomingFrame,
              (OfflineFrame & offline_frame,
               const std::string& from_endpoint_id, ClientProxy* to_client,
               Medium current_medium, PacketMetaData& packet_meta_data),
              (override));

  MOCK_METHOD(void, OnEndpointDisconnect,
              (ClientProxy * client, const std::string& service_id,
               const std::string& endpoint_id, CountDownLatch barrier,
               DisconnectionReason reason),
              (override));
};

class SetSafeToDisconnect {
 public:
  SetSafeToDisconnect(bool safe_to_disconnect, bool auto_reconnect,
                               bool payload_received_ack,
                               std::int32_t safe_to_disconnect_version) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableSafeToDisconnect,
        safe_to_disconnect);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableAutoReconnect,
        auto_reconnect);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnablePayloadReceivedAck,
        payload_received_ack);
    NearbyFlags::GetInstance().OverrideInt64FlagValue(
        config_package_nearby::nearby_connections_feature::
            kSafeToDisconnectVersion,
        safe_to_disconnect_version);
  }
};

class TestEndpointManager : public EndpointManager {
 public:
  TestEndpointManager(EndpointChannelManager* manager,
                      std::unique_ptr<SingleThreadExecutor> serial_executor)
      : EndpointManager(manager, std::move(serial_executor)) {}
};

class EndpointManagerTest : public ::testing::Test {
 protected:
  void RegisterEndpoint(std::unique_ptr<MockEndpointChannel> channel,
                        bool should_close = true) {
    CountDownLatch done(1);
    if (should_close) {
      ON_CALL(*channel, Close(_))
          .WillByDefault(
              [&done](DisconnectionReason reason) { done.CountDown(); });
    }
    EXPECT_CALL(*channel, GetMedium()).WillRepeatedly(Return(Medium::BLE));
    EXPECT_CALL(*channel, GetLastReadTimestamp())
        .WillRepeatedly(Return(start_time_));
    EXPECT_CALL(*channel, GetLastWriteTimestamp())
        .WillRepeatedly(Return(start_time_));
    EXPECT_CALL(mock_listener_.initiated_cb, Call).Times(1);
    em_.RegisterEndpoint(client_.get(), endpoint_id_, info_,
                         connection_options_, std::move(channel), listener_,
                         connection_token_);
    if (should_close) {
      EXPECT_TRUE(done.Await(absl::Milliseconds(1000)).result());
    }
  }
  SetSafeToDisconnect set_safe_to_disconnect_{true, false, true, 5};
  std::unique_ptr<ClientProxy> client_ = std::make_unique<ClientProxy>();
  ConnectionOptions connection_options_{
      .keep_alive_interval_millis = 5000,
      .keep_alive_timeout_millis = 30000,
  };
  std::vector<std::unique_ptr<EndpointManager::FrameProcessor>> processors_;
  EndpointChannelManager ecm_;
  EndpointManager em_{&ecm_};
  std::string endpoint_id_ = "endpoint_id";
  ConnectionResponseInfo info_ = {
      .remote_endpoint_info = ByteArray{"info"},
      .authentication_token = "auth_token",
      .raw_authentication_token = ByteArray{"auth_token"},
      .is_incoming_connection = true,
  };
  struct MockConnectionListener {
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const ConnectionResponseInfo& info)>>
        initiated_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>> accepted_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const Status& status)>>
        rejected_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>>
        disconnected_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 std::int32_t quality)>>
        bandwidth_changed_cb;
  } mock_listener_;
  ConnectionListener listener_{
      .initiated_cb = mock_listener_.initiated_cb.AsStdFunction(),
      .accepted_cb = mock_listener_.accepted_cb.AsStdFunction(),
      .rejected_cb = mock_listener_.rejected_cb.AsStdFunction(),
      .disconnected_cb = mock_listener_.disconnected_cb.AsStdFunction(),
      .bandwidth_changed_cb =
          mock_listener_.bandwidth_changed_cb.AsStdFunction(),
  };
  std::string connection_token_ = "conntokn";
  absl::Time start_time_{absl::Now()};
};

TEST_F(EndpointManagerTest, ConstructorDestructorWorks) { SUCCEED(); }

TEST_F(EndpointManagerTest, RegisterEndpointCallsOnConnectionInitiated) {
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read())
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, Close(_)).Times(1);
  RegisterEndpoint(std::move(endpoint_channel));
}

TEST_F(EndpointManagerTest, UnregisterEndpointCallsOnDisconnected) {
//  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
//  EXPECT_CALL(*endpoint_channel, Read())
//      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  RegisterEndpoint(std::make_unique<MockEndpointChannel>());
  // NOTE: disconnect_cb is not called, because we did not reach fully connected
  // state. On top of that, UnregisterEndpoint is suppressing this notification.
  // (IMO, it should be called as long as any connection callback was called
  // before. (in this case initiated_cb is called)).
  // Test captures current protocol behavior.
  em_.UnregisterEndpoint(client_.get(), endpoint_id_);
}

TEST_F(EndpointManagerTest,
       UnregisterEndpointCallsOnDisconnectedSafeToDisconnect) {
  RegisterEndpoint(std::make_unique<MockEndpointChannel>());
  // NOTE: disconnect_cb is not called, because we did not reach fully connected
  // state. On top of that, UnregisterEndpoint is suppressing this notification.
  // (IMO, it should be called as long as any connection callback was called
  // before. (in this case initiated_cb is called)).
  // Test captures current protocol behavior.
  client_->SetRemoteSafeToDisconnectVersion(endpoint_id_, 5);
  ecm_.UpdateSafeToDisconnectForEndpoint(endpoint_id_, true);
  em_.UnregisterEndpoint(client_.get(), endpoint_id_);
}

TEST_F(EndpointManagerTest, RegisterFrameProcessorWorks) {
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  auto connect_request = std::make_unique<MockFrameProcessor>();
  ByteArray endpoint_info{"endpoint_name"};
  ConnectionInfo connection_info{
      "endpoint_id",
      endpoint_info,
      1234 /*nonce*/,
      false /*supports_5_ghz*/,
      "" /*bssid*/,
      2412 /*ap_frequency*/,
      "8xqT" /*ip_address in 4 bytes format*/,
      std::vector<Medium>{Medium::BLE} /*supported_mediums*/,
      0 /*keep_alive_interval_millis*/,
      0 /*keep_alive_timeout_millis*/};

  auto read_data = parser::ForConnectionRequestConnections({}, connection_info);
  EXPECT_CALL(*connect_request, OnIncomingFrame);
  EXPECT_CALL(*connect_request, OnEndpointDisconnect);
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(Return(ExceptionOr<ByteArray>(read_data)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  // Register frame processor, then register endpoint.
  // Endpoint will read one frame, then fail to read more and terminate.
  // On disconnection, it will notify frame processor and we verify that.
  em_.RegisterFrameProcessor(V1Frame::CONNECTION_REQUEST,
                             connect_request.get());
  processors_.emplace_back(std::move(connect_request));
  RegisterEndpoint(std::move(endpoint_channel));
}

TEST_F(EndpointManagerTest, UnregisterFrameProcessorWorks) {
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read())
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));

  // We should not receive any notifications to frame processor.
  auto connect_request = std::make_unique<StrictMock<MockFrameProcessor>>();

  // Register frame processor and immediately unregister it.
  em_.RegisterFrameProcessor(V1Frame::CONNECTION_REQUEST,
                             connect_request.get());
  em_.UnregisterFrameProcessor(V1Frame::CONNECTION_REQUEST,
                               connect_request.get());

  processors_.emplace_back(std::move(connect_request));
  // Endpoint will not send OnDisconnect notification to frame processor.
  RegisterEndpoint(std::move(endpoint_channel), false);
  em_.UnregisterEndpoint(client_.get(), endpoint_id_);
}

TEST_F(EndpointManagerTest, SendControlMessageAndPayloadAckWorks) {
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  PayloadTransferFrame::PayloadHeader header;
  PayloadTransferFrame::ControlMessage control;
  header.set_id(12345);
  header.set_type(PayloadTransferFrame::PayloadHeader::BYTES);
  header.set_total_size(1024);
  control.set_offset(150);
  control.set_event(PayloadTransferFrame::ControlMessage::PAYLOAD_CANCELED);

  ON_CALL(*endpoint_channel, Read(_))
      .WillByDefault([channel = endpoint_channel.get()]() {
        if (channel->IsClosed()) return ExceptionOr<ByteArray>(Exception::kIo);
        NEARBY_LOGS(INFO) << "Simulate read delay: wait";
        absl::SleepFor(absl::Milliseconds(100));
        NEARBY_LOGS(INFO) << "Simulate read delay: done";
        if (channel->IsClosed()) return ExceptionOr<ByteArray>(Exception::kIo);
        return ExceptionOr<ByteArray>(ByteArray{});
      });
  ON_CALL(*endpoint_channel, Close(_))
      .WillByDefault(
          [channel = endpoint_channel.get()](DisconnectionReason reason) {
            channel->DoClose();
            NEARBY_LOGS(INFO) << "Channel closed";
          });
  EXPECT_CALL(*endpoint_channel, Write(_, _))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));

  RegisterEndpoint(std::move(endpoint_channel), false);
  auto failed_ids_1 =
      em_.SendControlMessage(header, control, std::vector{endpoint_id_});
  EXPECT_EQ(failed_ids_1, std::vector<std::string>{});
  auto failed_ids_2 = em_.SendPayloadAck(header.id(),
      std::vector<std::string>{endpoint_id_});
  EXPECT_EQ(failed_ids_2, std::vector<std::string>{});
  NEARBY_LOGS(INFO) << "Will unregister endpoint now";
  em_.UnregisterEndpoint(client_.get(), endpoint_id_);
  NEARBY_LOGS(INFO) << "Will call destructors now";
}

TEST_F(EndpointManagerTest, SingleReadOnReadError) {
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(
          Return(ExceptionOr<ByteArray>(Exception::kInvalidProtocolBuffer)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*endpoint_channel, Close(_)).Times(1);
  RegisterEndpoint(std::move(endpoint_channel));
}

TEST_F(EndpointManagerTest, ReadInvalidUnencryptedPayloadIgnoresFrame) {
  // 1. EndpointChannel is unencrypted.
  // 2. EndpointManager receives an invalid unencrypted frame.
  // 3. EndpointManager calls EndpointChannel::TryDecrypt(), which keeps failing
  // because the channel is not encrypted.
  // 4. Invalid frame is ignored. No bad side effects.
  CountDownLatch latch(1);
  const ByteArray payload("not a valid frame");
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(Return(ExceptionOr<ByteArray>(payload)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, TryDecrypt(Eq(payload)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kFailed)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  EXPECT_CALL(*endpoint_channel, Close(_))
      .WillOnce([&](DisconnectionReason reason) { latch.CountDown(); });
  RegisterEndpoint(std::move(endpoint_channel), false);
  latch.Await();
  em_.UnregisterEndpoint(client_.get(), endpoint_id_);
}

TEST_F(EndpointManagerTest, ReadInvalidEncryptedPayloadIgnoresFrame) {
  // 1. EndpointChannel is unencrypted.
  // 2. EndpointManager receives an invalid encrypted frame.
  // 3. EndpointManager calls EndpointChannel::TryDecrypt(), decryption fails
  // too.
  // 4. Invalid frame is ignored. No bad side effects.
  const ByteArray payload("not a valid frame");
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(Return(ExceptionOr<ByteArray>(payload)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, TryDecrypt(Eq(payload)))
      .WillOnce(Return(ExceptionOr<ByteArray>(Exception::kFailed)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kExecution)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  RegisterEndpoint(std::move(endpoint_channel));
}

TEST_F(EndpointManagerTest, ReadInvalidPayloadFromEncryptedChannel) {
  // 1. EndpointChannel is encrypted.
  // 2. EndpointManager receives an invalid encrypted frame.
  // 3. No calls to TryDecrypt.
  const ByteArray payload("not a valid frame");
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(Return(ExceptionOr<ByteArray>(payload)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, IsEncrypted()).WillRepeatedly(Return(true));
  EXPECT_CALL(*endpoint_channel, TryDecrypt(Eq(payload))).Times(0);
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  RegisterEndpoint(std::move(endpoint_channel));
}

TEST_F(EndpointManagerTest, TryDecrypt) {
  // 1. EndpointChannel is unencrypted.
  // 2. EndpointManager receives a valid encrypted frame alas it's interpreted
  // as unencrypted at first.
  // 3. EndpointManager calls EndpointChannel::TryDecrypt(), decryption works.
  // 4. Frame is processed.
  const ByteArray payload("valid encrypted frame");
  auto endpoint_channel = std::make_unique<MockEndpointChannel>();
  auto connect_request = std::make_unique<MockFrameProcessor>();
  ByteArray endpoint_info{"endpoint_name"};
  ConnectionInfo connection_info{
      "endpoint_id",
      endpoint_info,
      1234 /*nonce*/,
      false /*supports_5_ghz*/,
      "" /*bssid*/,
      2412 /*ap_frequency*/,
      "8xqT" /*ip_address in 4 bytes format*/,
      std::vector<Medium>{Medium::BLE} /*supported_mediums*/,
      0 /*keep_alive_interval_millis*/,
      0 /*keep_alive_timeout_millis*/};
  ByteArray decrypted_data =
      parser::ForConnectionRequestConnections({}, connection_info);
  EXPECT_CALL(*connect_request, OnIncomingFrame);
  EXPECT_CALL(*connect_request, OnEndpointDisconnect);
  EXPECT_CALL(*endpoint_channel, Read(_))
      .WillOnce(Return(ExceptionOr<ByteArray>(payload)))
      .WillRepeatedly(Return(ExceptionOr<ByteArray>(Exception::kIo)));
  EXPECT_CALL(*endpoint_channel, TryDecrypt(Eq(payload)))
      .WillOnce(Return(ExceptionOr<ByteArray>(Exception::kFailed)))
      .WillOnce(Return(ExceptionOr<ByteArray>(decrypted_data)));
  EXPECT_CALL(*endpoint_channel, Write(_))
      .WillRepeatedly(Return(Exception{Exception::kSuccess}));
  em_.RegisterFrameProcessor(V1Frame::CONNECTION_REQUEST,
                             connect_request.get());
  processors_.emplace_back(std::move(connect_request));
  RegisterEndpoint(std::move(endpoint_channel));
}

// Regression test for b/278729669.
//
// During the destruction of NearbyConnections, Core (which owns ClientProxy)
// is destructed before ServiceController (which owns EndpointManager), which
// means any pending tasks on the EndpointManager than use ClientProxy will
// be using garbage memory, and cause crashes. This test enforces the fix.
TEST_F(EndpointManagerTest, DisconnectEndpointDuringDestruction) {
  // This test uses a `FakeSingleThreadExecutor` in order to control when
  // tasks are executed in order to simulate the scenario where
  // `DiscardEndpoint` is posted to the executor before the EndpointManager
  // is destructed, and executed during it's destruction.
  std::unique_ptr<SingleThreadExecutor> serial_executor =
      std::make_unique<FakeSingleThreadExecutor>();
  FakeSingleThreadExecutor* fake_serial_executor =
      static_cast<FakeSingleThreadExecutor*>(serial_executor.get());
  std::unique_ptr<EndpointManager> endpoint_manager =
      std::make_unique<TestEndpointManager>(&ecm_, std::move(serial_executor));

  // DiscardEndpoint posts a task to the executor to run "discard-endpoint",
  // however the `FakeSingleThreadExecutor` will not run this task
  // immediately.
  fake_serial_executor->SetRunExecutablesImmediately(
      /*run_executables_immediately=*/false);
  endpoint_manager->DiscardEndpoint(client_.get(), endpoint_id_,
                                    DisconnectionReason::IO_ERROR);

  // Simulate Core destruction of ClientProxy by destroying `client_`.
  client_.reset();

  // Simulate ServiceController destruction of EndpointManager by destroying
  // `endpoint_manager`, and set the `FakeSingleThreadExecutor` to run
  // executables on calls `Execute`. When `endpoint_manager` is destructed, it
  // will block on calls to `Execute` to run all pending executables, notably
  // "discard-endpoint" from above. However, "discard-endpoint" will have a
  // reference to a destroyed ClientProxy.
  //
  // Expect no crash when "discard-endpoints" is executed during the
  // destruction.
  fake_serial_executor->SetRunExecutablesImmediately(
      /*run_executables_immediately=*/true);
  endpoint_manager.reset();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
