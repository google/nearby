#include "core_v2/internal/mediums/webrtc.h"

#include "core_v2/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "platform_v2/base/listeners.h"
#include "platform_v2/public/mutex_lock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

// Basic test to check that device is accepting connections when initialized.
TEST(WebRtcTest, NotAcceptingConnections) {
  WebRtc webrtc;
  ASSERT_TRUE(webrtc.IsAvailable());
  EXPECT_FALSE(webrtc.IsAcceptingConnections());
}

// Tests the flow when the device tries to accept connections twice. In this
// case, only the first call is successful and subsequent calls fail.
TEST(WebRtcTest, StartAcceptingConnectionTwice) {
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
TEST(WebRtcTest, Connect_DataChannelTimeOut) {
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
TEST(WebRtcTest, StartAcceptingConnection_ThenConnect) {
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
TEST(WebRtcTest, StartAndStopAcceptingConnections) {
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

// Tests the flow when the device calls StartAcceptingConnections() after
// calling Connect() without disconnecting in between.
TEST(WebRtcTest, Connect_ThenStartAcceptingConnections) {
  // TODO(himanshujaju) - Complete the test.
}

// Tests the flow when the device tries to connect to two different peers
// without disconnecting in between.
TEST(WebRtcTest, ConnectTwice) {
  // TODO(himanshujaju) - Complete the test.
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other but disconnect before being able to send/receive the actual data.
TEST(WebRtcTest, ConnectBothDevicesAndAbort) {
  // TODO(himanshujaju) - Complete the test.
}

// Tests the flow when the two devices exchange SDP messages and connect to each
// other and the actual data is exchanged successfully between the devices.
TEST(WebRtcTest, ConnectBothDevicesAndSendData) {
  // TODO(himanshujaju) - Complete the test.
}

}  // namespace

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
