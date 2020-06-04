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

#include "core_v2/internal/mediums/webrtc/webrtc_socket.h"

#include <memory>

#include "platform_v2/base/byte_array.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "webrtc/api/data_channel_interface.h"

namespace location {
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
  const ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.NotifyDataChannelMsgReceived(kMessage);
  ExceptionOr<ByteArray> result = webrtc_socket.GetInputStream().Read(7);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), kMessage);
}

TEST(WebRtcSocketTest, ReadMultipleMessages) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.NotifyDataChannelMsgReceived(ByteArray{"Me"});
  webrtc_socket.NotifyDataChannelMsgReceived(ByteArray{"ssa"});
  webrtc_socket.NotifyDataChannelMsgReceived(ByteArray{"ge"});

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
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(webrtc_socket.GetOutputStream().Write(kMessage).Ok());
}

TEST(WebRtcSocketTest, SendDataBiggerThanMax) {
  const ByteArray kMessage{kMaxDataSize + 1};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, WriteToDataChannelFails) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(false));
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, Close) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Close());

  int socket_closed_cb_called = 0;

  webrtc_socket.SetOnSocketClosedListener(
      {.socket_closed_cb = [&]() { socket_closed_cb_called++; }});
  webrtc_socket.Close();

  EXPECT_EQ(socket_closed_cb_called, 1);
}

TEST(WebRtcSocketTest, WriteOnClosedChannel) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);
  webrtc_socket.Close();

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.GetOutputStream().Write(kMessage),
            Exception{Exception::kIo});
}

TEST(WebRtcSocketTest, ReadFromClosedChannel) {
  ByteArray kMessage{"Message"};
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket webrtc_socket(kSocketName, mock_data_channel);
  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(true));

  webrtc_socket.GetOutputStream().Write(kMessage);
  webrtc_socket.Close();

  EXPECT_EQ(webrtc_socket.GetInputStream().Read(7).exception(), Exception::kIo);
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
