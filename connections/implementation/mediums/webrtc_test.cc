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

#include "connections/implementation/mediums/webrtc.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/mediums/webrtc_socket.h"
#include "internal/platform/listeners.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {

using FeatureFlags = FeatureFlags::Flags;
using ::location::nearby::connections::LocationHint;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

class WebRtcTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using MockAcceptedCallback = testing::MockFunction<void(
      const std::string& service_id, WebRtcSocketWrapper socket)>;

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but the signaling channel is closed before sending the data.
TEST_P(WebRtcTest, ConnectBothDevices_ShutdownSignaling_SendData) {
  env_.Start({.webrtc_enabled = true});
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const WebrtcPeerId self_id("self_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](const std::string& service_id,
                                     WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  CancellationFlag flag;
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
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
  env_.Stop();
}

TEST_P(WebRtcTest, CanCancelConnect) {
  env_.Start({.webrtc_enabled = true});
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const WebrtcPeerId self_id("self_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](const std::string& service_id,
                                     WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  CancellationFlag flag(true);
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
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
  } else {
    EXPECT_FALSE(sender_socket.IsValid());
  }
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedWebRtcTest, WebRtcTest,
                         ::testing::ValuesIn(kTestCases));

// Basic test to check that device is accepting connections when initialized.
TEST_F(WebRtcTest, NotAcceptingConnections) {
  env_.Start({.webrtc_enabled = true});
  WebRtc webrtc;
  ASSERT_TRUE(webrtc.IsAvailable());
  EXPECT_FALSE(webrtc.IsAcceptingConnections(std::string{}));
  env_.Stop();
}

// Tests the flow when the device tries to accept connections twice. In this
// case, only the first call is successful and subsequent calls fail.
TEST_F(WebRtcTest, StartAcceptingConnectionTwice) {
  env_.Start({.webrtc_enabled = true});
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  WebRtc webrtc;
  WebrtcPeerId self_id("peer_id");
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
  env_.Stop();
}

// Tests the flow when the device tries to connect but there is no peer
// accepting connections at the given peer ID.
TEST_F(WebRtcTest, Connect_NoPeer) {
  env_.Start({.webrtc_enabled = true});
  WebRtc webrtc;
  WebrtcPeerId peer_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper_1 =
      webrtc.Connect(service_id, peer_id, location_hint, &flag);
  EXPECT_FALSE(wrapper_1.IsValid());

  EXPECT_TRUE(webrtc.StartAcceptingConnections(
      service_id, peer_id, location_hint, AcceptedConnectionCallback()));
  env_.Stop();
}

// Tests the flow when the device calls Connect() after calling
// StartAcceptingConnections() without StopAcceptingConnections().
TEST_F(WebRtcTest, StartAcceptingConnection_ThenConnect) {
  env_.Start({.webrtc_enabled = true});
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  WebRtc webrtc;
  WebrtcPeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper = webrtc.Connect(
      service_id, WebrtcPeerId("random_peer_id"), location_hint, &flag);
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));
  EXPECT_FALSE(wrapper.IsValid());
  EXPECT_FALSE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  env_.Stop();
}

// Tests the flow when the device calls StartAcceptingConnections but the medium
// is closed before a peer device can connect to it.
TEST_F(WebRtcTest, StartAndStopAcceptingConnections) {
  env_.Start({.webrtc_enabled = true});
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  WebRtc webrtc;
  WebrtcPeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));
  webrtc.StopAcceptingConnections(service_id);
  EXPECT_FALSE(webrtc.IsAcceptingConnections(service_id));
  env_.Stop();
}

// Tests the flow when the device tries to connect to two different peers
// without disconnecting in between.
TEST_F(WebRtcTest, ConnectTwice) {
  env_.Start({.webrtc_enabled = true});
  WebRtc receiver, sender, device_c;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const WebrtcPeerId self_id("self_id"), other_id("other_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](const std::string& service_id,
                                     WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  device_c.StartAcceptingConnections(
      service_id, other_id, location_hint,
      {[](const std::string& service_id, WebRtcSocketWrapper wrapper) {}});

  CancellationFlag flag;
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  WebRtcSocketWrapper socket =
      sender.Connect(service_id, other_id, location_hint, &flag);
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
  env_.Stop();
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but disconnect before being able to send/receive the actual data.
TEST_F(WebRtcTest, ConnectBothDevicesAndAbort) {
  env_.Start({.webrtc_enabled = true});
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const WebrtcPeerId self_id("self_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message xyz");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](const std::string& service_id,
                                     WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  CancellationFlag flag;
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
  EXPECT_TRUE(sender_socket.IsValid());

  ExceptionOr<bool> devices_connected = connected.Get();
  ASSERT_TRUE(devices_connected.ok());
  EXPECT_TRUE(devices_connected.result());

  receiver_socket.Close();
  env_.Stop();
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other and the actual data is exchanged successfully between the devices.
TEST_F(WebRtcTest, ConnectBothDevicesAndSendData) {
  env_.Start({.webrtc_enabled = true});
  WebRtc receiver, sender;
  WebRtcSocketWrapper receiver_socket, sender_socket;
  const WebrtcPeerId self_id("self_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;
  Future<bool> connected;
  ByteArray message("message");

  receiver.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {[&receiver_socket, connected](const std::string& service_id,
                                     WebRtcSocketWrapper wrapper) mutable {
        receiver_socket = wrapper;
        connected.Set(receiver_socket.IsValid());
      }});

  CancellationFlag flag;
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
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
  env_.Stop();
}

TEST_F(WebRtcTest, Connect_NullPeerConnection) {
  env_.Start({.webrtc_enabled = true});
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  env_.SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  const std::string service_id("NearbySharing");
  WebrtcPeerId self_id("peer_id");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper = webrtc.Connect(
      service_id, WebrtcPeerId("random_peer_id"), location_hint, &flag);
  EXPECT_FALSE(wrapper.IsValid());
  env_.Stop();
}

// Tests the flow when the device calls StartAcceptingConnections and the
// receive messages stream fails.
TEST_F(WebRtcTest, ContinueAcceptingConnectionsOnComplete) {
  env_.Start({.webrtc_enabled = true});
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;
  WebRtc webrtc;
  WebrtcPeerId self_id("peer_id");
  const std::string service_id("NearbySharing");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  ASSERT_TRUE(webrtc.StartAcceptingConnections(
      service_id, self_id, location_hint,
      {mock_accepted_callback_.AsStdFunction()}));
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));

  // Simulate a failure in receiving messages stream, WebRtc should restart
  // accepting connections.
  env_.SendWebRtcSignalingComplete(self_id.GetId(),
                                   /*success=*/false);
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));

  // And a "success" message should not cause accepting connections to stop.
  env_.SendWebRtcSignalingComplete(self_id.GetId(),
                                   /*success=*/true);
  EXPECT_TRUE(webrtc.IsAcceptingConnections(service_id));

  webrtc.StopAcceptingConnections(service_id);
  EXPECT_FALSE(webrtc.IsAcceptingConnections(service_id));
  env_.Stop();
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
