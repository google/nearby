#ifndef PLATFORM_V2_IMPL_G3_WEBRTC_H_
#define PLATFORM_V2_IMPL_G3_WEBRTC_H_

#include <memory>

#include "platform_v2/api/webrtc.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace g3 {

class WebRtcMedium : public api::WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() = default;
  ~WebRtcMedium() override = default;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id) override;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_WEBRTC_H_
