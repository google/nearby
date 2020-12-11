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

#include "core/internal/mediums/webrtc.h"

#include "core/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "platform/base/listeners.h"
#include "platform/base/medium_environment.h"
#include "platform/public/mutex_lock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {
const int kTwoMBSize = 2000000;
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
  EXPECT_FALSE(webrtc.IsAcceptingConnections(std::string{}));
}

// Tests the flow when the device tries to accept connections twice. In this
// case, only the first call is successful and subsequent calls fail.
TEST_F(WebRtcTest, StartAcceptingConnectionTwice) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint{};

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));
  EXPECT_FALSE(webrtc.IsAcceptingConnections(std::string{}));
}

// Tests the flow when the device tries to connect but the data channel times
// out.
TEST_F(WebRtcTest, Connect_DataChannelTimeOut) {
  WebRtc webrtc;
  PeerId peer_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  WebRtcSocketWrapper wrapper_1 = webrtc.Connect(peer_id, location_hint);
  EXPECT_FALSE(wrapper_1.IsValid());

  EXPECT_TRUE(webrtc.StartAcceptingConnections(
      service_id, peer_id, location_hint, AcceptedConnectionCallback()));
}

// Tests the flow when the device calls Connect() after calling
// StartAcceptingConnections() without StopAcceptingConnections().
TEST_F(WebRtcTest, StartAcceptingConnection_ThenConnect) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  WebRtcSocketWrapper wrapper =
      webrtc.Connect(PeerId("random_peer_id"), location_hint);
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));
  EXPECT_FALSE(wrapper.IsValid());
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
}

// Tests the flow when the device calls StartAcceptingConnections but the medium
// is closed before a peer device can connect to it.
TEST_F(WebRtcTest, StartAndStopAcceptingConnections) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  WebRtc webrtc;
  PeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));
  webrtc.StopAcceptingConnections(service_id);
  EXPECT_FALSE(webrtc.IsAcceptingConnections(service_id));
}

// Tests the flow when the device tries to connect to two different peers
// without disconnecting in between.
TEST_F(WebRtcTest, ConnectTwice) {
  WebRtc receiver, sender, device_c;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const PeerId self_id("self_id"), other_id("other_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  device_c.StartAcceptingConnections(service_id, other_id, location_hint,
                                     {[](WebRtcSocketWrapper wrapper) {}});

  sender_socket = sender.Connect(self_id, location_hint);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  WebRtcSocketWrapper socket = sender.Connect(other_id, location_hint);
  EXPECT_TRUE(socket.IsValid());
  socket.Close();

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
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id, location_hint);
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
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id, location_hint);
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
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  sender_socket = sender.Connect(self_id, location_hint);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  // Only shuts down signaling channel.
  receiver.StopAcceptingConnections(service_id);

  sender_socket.GetOutputStream().Write(message);
  ExceptionOr<ByteArray> received_msg =
      receiver_socket.GetInputStream().Read(/*size=*/32);
  ASSERT_TRUE(received_msg.ok());
  EXPECT_EQ(message, received_msg.result());
}

// Tests the flow when the two devices created two data channel and transfer
// data in the same time.
TEST_F(WebRtcTest, TwoChannels_SendData) {
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket1, receiver_socket2, sender_socket1,
      sender_socket2;
  const PeerId self_id1("self_id1"), self_id2("self_id2");
  const std::string service_id1("service1"), service_id2("service2");
  LocationHint location_hint;
  Future<bool> connected1, connected2;
  ByteArray message;
  message.SetData(kTwoMBSize / 10, 'c');

  receiver.StartAcceptingConnections(
      service_id1, self_id1, location_hint,
      {[&receiver_socket1, connected1](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket1 = wrapper;
        connected1.Set(receiver_socket1.IsValid());
      }});

  receiver.StartAcceptingConnections(
      service_id2, self_id2, location_hint,
      {[&receiver_socket2, connected2](WebRtcSocketWrapper wrapper) mutable {
        receiver_socket2 = wrapper;
        connected2.Set(receiver_socket2.IsValid());
      }});

  sender_socket1 = sender.Connect(self_id1, location_hint);
  EXPECT_TRUE(sender_socket1.IsValid());

  sender_socket2 = sender.Connect(self_id2, location_hint);
  EXPECT_TRUE(sender_socket2.IsValid());

  ExceptionOr<bool> devices_connected1 = connected1.Get();
  ASSERT_TRUE(devices_connected1.ok());
  EXPECT_TRUE(devices_connected1.result());

  ExceptionOr<bool> devices_connected2 = connected1.Get();
  ASSERT_TRUE(devices_connected2.ok());
  EXPECT_TRUE(devices_connected2.result());

  // Only shuts down signaling channel.
  receiver.StopAcceptingConnections(service_id1);
  receiver.StopAcceptingConnections(service_id2);

  for (int i = 0; i < 10; i++) {
    sender_socket1.GetOutputStream().Write(message);
    sender_socket2.GetOutputStream().Write(message);
    ExceptionOr<ByteArray> received_msg1 =
        receiver_socket1.GetInputStream().Read(kTwoMBSize / 10);
    ASSERT_TRUE(received_msg1.ok());
    ExceptionOr<ByteArray> received_msg2 =
        receiver_socket2.GetInputStream().Read(kTwoMBSize / 10);
    EXPECT_EQ(message, received_msg1.result());
    EXPECT_EQ(message, received_msg2.result());
  }
}

TEST_F(WebRtcTest, StartAcceptingConnections_NullPeerConnection) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  PeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
}

TEST_F(WebRtcTest, Connect_NullPeerConnection) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  PeerId self_id("peer_id");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  WebRtcSocketWrapper wrapper =
      webrtc.Connect(PeerId("random_peer_id"), location_hint);
  EXPECT_FALSE(wrapper.IsValid());
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
