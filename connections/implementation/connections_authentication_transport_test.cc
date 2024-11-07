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

#include "connections/implementation/connections_authentication_transport.h"

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::testing::_;

class MockEndpointChannel : public EndpointChannel {
 public:
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (), (override));
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (PacketMetaData&), (override));
  MOCK_METHOD(Exception, Write, (const ByteArray& data), (override));
  MOCK_METHOD(Exception, Write, (const ByteArray&, PacketMetaData&),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(
      void, Close,
      (location::nearby::proto::connections::DisconnectionReason reason),
      (override));
  MOCK_METHOD(void, Close,
              (location::nearby::proto::connections::DisconnectionReason reason,
               location::nearby::analytics::proto::ConnectionsLog::
                   EstablishedConnection::SafeDisconnectionResult result),
              (override));
  MOCK_METHOD(bool, IsClosed, (), (const, override));
  MOCK_METHOD(std::string, GetType, (), (const, override));
  MOCK_METHOD(std::string, GetServiceId, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(location::nearby::proto::connections::Medium, GetMedium, (),
              (const, override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionTechnology,
              GetTechnology, (), (const, override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionBand, GetBand, (),
              (const, override));
  MOCK_METHOD(int, GetFrequency, (), (const, override));
  MOCK_METHOD(int, GetTryCount, (), (const, override));
  MOCK_METHOD(int, GetMaxTransmitPacketSize, (), (const, override));
  MOCK_METHOD(void, EnableEncryption, (std::shared_ptr<EncryptionContext>),
              (override));
  MOCK_METHOD(void, DisableEncryption, (), (override));
  MOCK_METHOD(bool, IsEncrypted, (), (override));
  MOCK_METHOD(ExceptionOr<ByteArray>, TryDecrypt, (const ByteArray& data),
              (override));
  MOCK_METHOD(bool, IsPaused, (), (const, override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(absl::Time, GetLastReadTimestamp, (), (const, override));
  MOCK_METHOD(absl::Time, GetLastWriteTimestamp, (), (const, override));
  MOCK_METHOD(void, SetAnalyticsRecorder,
              (analytics::AnalyticsRecorder*, const std::string&), (override));

  std::vector<std::string> messages_;
};

TEST(ConnectionsAuthenticationTransportTest, TestWriteMessage) {
  MockEndpointChannel channel;
  ConnectionsAuthenticationTransport transport(channel);
  EXPECT_CALL(channel, Write(_)).WillOnce([&channel](const ByteArray& data) {
    channel.messages_.push_back(data.string_data());
    return Exception{
        .value = Exception::Value::kSuccess,
    };
  });
  transport.WriteMessage("hello world");
  EXPECT_THAT(channel.messages_, testing::ElementsAre("hello world"));
}

TEST(ConnectionsAuthenticationTransportTest, TestReadMessage) {
  MockEndpointChannel channel;
  ConnectionsAuthenticationTransport transport(channel);
  channel.messages_.push_back("hello world");
  EXPECT_CALL(channel, Read()).WillOnce([&channel]() {
    std::string ret = channel.messages_[0];
    channel.messages_.erase(channel.messages_.begin());
    return ExceptionOr<ByteArray>(ByteArray(ret));
  });
  EXPECT_EQ(transport.ReadMessage(), "hello world");
}

TEST(ConnectionsAuthenticationTransportTest, TestReadMessageFail) {
  MockEndpointChannel channel;
  ConnectionsAuthenticationTransport transport(channel);
  channel.messages_.push_back("hello world");
  EXPECT_CALL(channel, Read()).WillOnce([]() {
    return ExceptionOr<ByteArray>(Exception::Value::kIo);
  });
  EXPECT_EQ(transport.ReadMessage(), "");
}

}  // namespace
}  // namespace connections
}  // namespace nearby
