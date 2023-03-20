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

#include "connections/implementation/mediums/webrtc/webrtc_socket_impl.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "webrtc/api/data_channel_interface.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {

// using TestPlatform = platform::ImplementationPlatform;

const char kSocketName[] = "TestSocket";

class MockDataChannel
    : public rtc::RefCountedObject<webrtc::DataChannelInterface> {
 public:
  MOCK_METHOD(void, RegisterObserver, (webrtc::DataChannelObserver*));
  MOCK_METHOD(void, UnregisterObserver, ());

  MOCK_METHOD(std::string, label, (), (const));

  MOCK_METHOD(bool, reliable, (), (const));
  MOCK_METHOD(int, id, (), (const));
  MOCK_METHOD(DataState, state, (), (const));
  MOCK_METHOD(uint32_t, messages_sent, (), (const));
  MOCK_METHOD(uint64_t, bytes_sent, (), (const));
  MOCK_METHOD(uint32_t, messages_received, (), (const));
  MOCK_METHOD(uint64_t, bytes_received, (), (const));

  MOCK_METHOD(uint64_t, buffered_amount, (), (const));

  MOCK_METHOD(void, Close, ());

  MOCK_METHOD(bool, Send, (const webrtc::DataBuffer&));
};

}  // namespace

TEST(WebRtcSocketTest, ReadFromSocket) {
  const char* message = "message";
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.OnMessage(webrtc::DataBuffer{message});
  ExceptionOr<ByteArray> result = webrtc_socket.GetInputStream().Read(7);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray{message});
}

TEST(WebRtcSocketTest, ReadMultipleMessages) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.OnMessage(webrtc::DataBuffer{"Me"});
  webrtc_socket.OnMessage(webrtc::DataBuffer{"ssa"});
  webrtc_socket.OnMessage(webrtc::DataBuffer{"ge"});

  ExceptionOr<ByteArray> result;

  // This behaviour is different from the Java code
  result = webrtc_socket.GetInputStream().Read(7);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray{"Me"});

  result = webrtc_socket.GetInputStream().Read(7);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray{"ssa"});

  result = webrtc_socket.GetInputStream().Read(7);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray{"ge"});
}

TEST(WebRtcSocketTest, WriteToSocket) {
  const ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(webrtc_socket.GetOutputStream().Write(kMessage).Ok());
}

TEST(WebRtcSocketTest, SendDataBiggerThanMax) {
  const ByteArray kMessage{kMaxDataSize + 1};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, WriteToDataChannelFails) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(false));
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, Close) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Close());

  int socket_closed_cb_called = 0;

  webrtc_socket.SetSocketListener(
      {.socket_closed_cb = [&](WebRtcSocket* socket) {
        socket_closed_cb_called++;
      }});
  webrtc_socket.Close();

  // We have to fake the close event to get the callback to run.
  ON_CALL(*mock_data_channel, state())
      .WillByDefault(
          testing::Return(webrtc::DataChannelInterface::DataState::kClosed));

  webrtc_socket.OnStateChange();

  EXPECT_EQ(socket_closed_cb_called, 1);
}

TEST(WebRtcSocketTest, WriteOnClosedChannel) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);
  webrtc_socket.Close();

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, ReadFromClosedChannel) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);
  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(true));

  webrtc_socket.GetOutputStream().Write(kMessage);
  webrtc_socket.Close();

  EXPECT_TRUE(webrtc_socket.GetInputStream().Read(7).GetResult().Empty());
}

TEST(WebRtcSocketTest, DataChannelCloseEventCleansUp) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  ON_CALL(*mock_data_channel, state())
      .WillByDefault(
          testing::Return(webrtc::DataChannelInterface::DataState::kClosed));

  webrtc_socket.OnStateChange();

  EXPECT_TRUE(webrtc_socket.GetInputStream().Read(7).GetResult().Empty());

  // Calling Close again should be safe even if the channel is already shut
  // down.
  webrtc_socket.Close();
}

TEST(WebRtcSocketTest, OpenStateTriggersCallback) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  int socket_ready_cb_called = 0;

  webrtc_socket.SetSocketListener(
      {.socket_ready_cb = [&](WebRtcSocket* socket) {
        socket_ready_cb_called++;
      }});

  ON_CALL(*mock_data_channel, state())
      .WillByDefault(
          testing::Return(webrtc::DataChannelInterface::DataState::kOpen));

  webrtc_socket.OnStateChange();

  EXPECT_EQ(socket_ready_cb_called, 1);
}

TEST(WebRtcSocketTest, CloseStateTriggersCallback) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel(new MockDataChannel());
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  int socket_closed_cb_called = 0;

  webrtc_socket.SetSocketListener(
      {.socket_closed_cb = [&](WebRtcSocket* socket) {
        socket_closed_cb_called++;
      }});

  ON_CALL(*mock_data_channel, state())
      .WillByDefault(
          testing::Return(webrtc::DataChannelInterface::DataState::kClosed));

  webrtc_socket.OnStateChange();

  EXPECT_EQ(socket_closed_cb_called, 1);
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
