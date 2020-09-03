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

#include "core_v2/internal/mediums/webrtc.h"

#include "core_v2/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "platform_v2/base/listeners.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/mutex_lock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

class WebRtcTest : public ::testing::Test {
 protected:
  WebRtcTest() {
    MediumEnvironment::Instance().Stop();
    MediumEnvironment::Instance().Start({.webrtc_enabled = true});
  }
};

// Basic test to check that device is accepting connections when initialized.
TEST_F(WebRtcTest, NotAcceptingConnections) {
  WebRtc webrtc;
  ASSERT_TRUE(webrtc.IsAvailable());
  EXPECT_FALSE(webrtc.IsAcceptingConnections());
}

// Tests the flow when the device tries to accept connections twice. In this
// case, only the first call is successful and subsequent calls fail.
TEST_F(WebRtcTest, StartAcceptingConnectionTwice) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_TRUE(webrtc.IsAcceptingConnections());
}

// Tests the flow when the device tries to connect but the data channel times
// out.
TEST_F(WebRtcTest, Connect_DataChannelTimeOut) {
  WebRtc webrtc;
  PeerId peer_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  WebRtcSocketWrapper wrapper_1 = webrtc.Connect(peer_id);
  EXPECT_FALSE(wrapper_1.IsValid());

  EXPECT_TRUE(
      webrtc.StartAcceptingConnections(peer_id, AcceptedConnectionCallback()));
}

// Tests the flow when the device calls Connect() after calling
// StartAcceptingConnections() without StopAcceptingConnections().
TEST_F(WebRtcTest, StartAcceptingConnection_ThenConnect) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
  WebRtcSocketWrapper wrapper = webrtc.Connect(PeerId("random_peer_id"));
  EXPECT_TRUE(webrtc.IsAcceptingConnections());
  EXPECT_FALSE(wrapper.IsValid());
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
}

// Tests the flow when the device calls StartAcceptingConnections but the medium
// is closed before a peer device can connect to it.
TEST_F(WebRtcTest, StartAndStopAcceptingConnections) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
  webrtc.StopAcceptingConnections();
  EXPECT_FALSE(webrtc.IsAcceptingConnections());
}

// Tests the flow when the device tries to connect to two different peers
// without disconnecting in between.
TEST_F(WebRtcTest, ConnectTwice) {
  WebRtc receiver, sender, device_c;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const PeerId self_id("self_id"), other_id("other_id");
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      self_id,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  device_c.StartAcceptingConnections(other_id,
                                     {mock_accepted_callback_.AsStdFunction()});

  sender_socket = sender.Connect(self_id);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  WebRtcSocketWrapper socket = sender.Connect(other_id);
  EXPECT_FALSE(socket.IsValid());

  EXPECT_TRUE(receiver_socket.IsValid());
  EXPECT_TRUE(sender_socket.IsValid());

  sender_socket.GetOutputStream().Write(message);
  ExceptionOr<ByteArray> received_msg =
      receiver_socket.GetInputStream().Read(/*size=*/32);
  ASSERT_TRUE(received_msg.ok());
  EXPECT_EQ(message, received_msg.result());

  receiver_socket.Close();
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but disconnect before being able to send/receive the actual data.
TEST_F(WebRtcTest, ConnectBothDevicesAndAbort) {
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const PeerId self_id("self_id");
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      self_id,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  receiver_socket.Close();
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other and the actual data is exchanged successfully between the devices.
TEST_F(WebRtcTest, ConnectBothDevicesAndSendData) {
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const PeerId self_id("self_id");
  Future<bool> connected;
  ByteArray message("message");

  receiver.StartAcceptingConnections(
      self_id,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  sender_socket.GetOutputStream().Write(message);
  ExceptionOr<ByteArray> received_msg =
      receiver_socket.GetInputStream().Read(/*size=*/32);
  ASSERT_TRUE(received_msg.ok());
  EXPECT_EQ(message, received_msg.result());

  receiver_socket.Close();
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but the signaling channel is closed before sending the data.
TEST_F(WebRtcTest, ConnectBothDevices_ShutdownSignaling_SendData) {
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const PeerId self_id("self_id");
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      self_id,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  // Only shuts down signaling channel.
  receiver.StopAcceptingConnections();

  sender_socket.GetOutputStream().Write(message);
  ExceptionOr<ByteArray> received_msg =
      receiver_socket.GetInputStream().Read(/*size=*/32);
  ASSERT_TRUE(received_msg.ok());
  EXPECT_EQ(message, received_msg.result());
}

TEST_F(WebRtcTest, StartAcceptingConnections_NullPeerConnection) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  PeerId self_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      self_id, {mock_accepted_callback_.AsStdFunction()}));
}

TEST_F(WebRtcTest, Connect_NullPeerConnection) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  PeerId self_id("peer_id");

  ASSERT_TRUE(webrtc.IsAvailable());
  WebRtcSocketWrapper wrapper = webrtc.Connect(PeerId("random_peer_id"));
  EXPECT_FALSE(wrapper.IsValid());
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
