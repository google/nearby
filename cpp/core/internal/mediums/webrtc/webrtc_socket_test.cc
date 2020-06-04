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

#include "core/internal/mediums/webrtc/webrtc_socket.h"

#include "platform/api/platform.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "webrtc/api/data_channel_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

using TestPlatform = platform::ImplementationPlatform;

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

class MockSocketClosedListener
    : public WebRtcSocket<TestPlatform>::SocketClosedListener {
 public:
  MOCK_METHOD(void, OnSocketClosed, ());
};

TEST(WebRtcSocketTest, ReadFromSocket) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray("Message"));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.NotifyDataChannelMsgReceived(kMessage);
  ExceptionOr<ConstPtr<ByteArray>> result =
      webrtc_socket.getInputStream()->read();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), kMessage);
}

TEST(WebRtcSocketTest, ReadMultipleMessages) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);

  webrtc_socket.NotifyDataChannelMsgReceived(MakeConstPtr(new ByteArray("Me")));
  webrtc_socket.NotifyDataChannelMsgReceived(
      MakeConstPtr(new ByteArray("ssa")));
  webrtc_socket.NotifyDataChannelMsgReceived(MakeConstPtr(new ByteArray("ge")));
  ExceptionOr<ConstPtr<ByteArray>> result;

  // This behaviour is different from the Java code
  result = webrtc_socket.getInputStream()->read();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result()->asString(), "Me");

  result = webrtc_socket.getInputStream()->read();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result()->asString(), "ssa");

  result = webrtc_socket.getInputStream()->read();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result()->asString(), "ge");
}

TEST(WebRtcSocketTest, WriteToSocket) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray("Message"));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(webrtc_socket.getOutputStream()->write(kMessage), Exception::NONE);
}

TEST(WebRtcSocketTest, SendDataBiggerThanMax) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray(kMaxDataSize + 1));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.getOutputStream()->write(kMessage), Exception::IO);
}

TEST(WebRtcSocketTest, WriteToDataChannelFails) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray("Message"));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);

  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(false));
  EXPECT_EQ(webrtc_socket.getOutputStream()->write(kMessage), Exception::IO);
}

TEST(WebRtcSocketTest, Close) {
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  ScopedPtr<Ptr<MockSocketClosedListener>> mock_listener(
      MakePtr(new MockSocketClosedListener()));
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);
  webrtc_socket.SetOnSocketClosedListener(mock_listener.get());

  EXPECT_CALL(*mock_listener, OnSocketClosed());
  EXPECT_CALL(*mock_data_channel, Close());
  webrtc_socket.close();
}

TEST(WebRtcSocketTest, WriteOnClosedChannel) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray("Message"));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);
  webrtc_socket.close();

  EXPECT_CALL(*mock_data_channel, Send(testing::_)).Times(0);
  EXPECT_EQ(webrtc_socket.getOutputStream()->write(kMessage), Exception::IO);
}

TEST(WebRtcSocketTest, ReadFromClosedChannel) {
  ConstPtr<ByteArray> kMessage = MakeConstPtr(new ByteArray("Message"));
  rtc::scoped_refptr<MockDataChannel> mock_data_channel = new MockDataChannel();
  WebRtcSocket<TestPlatform> webrtc_socket(kSocketName, mock_data_channel);
  ON_CALL(*mock_data_channel, Send(testing::_))
      .WillByDefault(testing::Return(true));

  webrtc_socket.getOutputStream()->write(kMessage);
  webrtc_socket.close();

  EXPECT_EQ(webrtc_socket.getInputStream()->read().exception(), Exception::IO);
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
