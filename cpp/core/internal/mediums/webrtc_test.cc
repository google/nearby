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

using FeatureFlags = FeatureFlags::Flags;

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
  WebRtcTest() {
    MediumEnvironment::Instance().Stop();
    MediumEnvironment::Instance().Start({.webrtc_enabled = true});
  }
};

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but the signaling channel is closed before sending the data.
TEST_P(WebRtcTest, ConnectBothDevices_ShutdownSignaling_SendData) {
  FeatureFlags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);
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
}

TEST_P(WebRtcTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);
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
}

INSTANTIATE_TEST_SUITE_P(ParametrisedWebRtcTest, WebRtcTest,
                         ::testing::ValuesIn(kTestCases));

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
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper_1 =
      webrtc.Connect(service_id, peer_id, location_hint, &flag);
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
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper = webrtc.Connect(
      service_id, PeerId("random_peer_id"), location_hint, &flag);
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

  CancellationFlag flag;
  sender_socket = sender.Connect(service_id, self_id, location_hint, &flag);
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
}

TEST_F(WebRtcTest, Connect_NullPeerConnection) {
  using MockAcceptedCallback =
      testing::MockFunction<void(WebRtcSocketWrapper socket)>;
  testing::StrictMock<MockAcceptedCallback> mock_accepted_callback_;

  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtc webrtc;
  const std::string service_id("NearbySharing");
  PeerId self_id("peer_id");
  LocationHint location_hint;

  ASSERT_TRUE(webrtc.IsAvailable());
  CancellationFlag flag;
  WebRtcSocketWrapper wrapper = webrtc.Connect(
      service_id, PeerId("random_peer_id"), location_hint, &flag);
  EXPECT_FALSE(wrapper.IsValid());
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
