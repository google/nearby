#ifndef PLATFORM_V2_PUBLIC_WEBRTC_H_
#define PLATFORM_V2_PUBLIC_WEBRTC_H_

#include <memory>

#include "platform_v2/api/platform.h"
#include "platform_v2/api/webrtc.h"
#include "webrtc/files/stable/webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {

class WebRtcMedium final {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() : impl_(api::ImplementationPlatform::CreateWebRtcMedium()) {}
  ~WebRtcMedium() = default;
  WebRtcMedium(WebRtcMedium&&) = delete;
  WebRtcMedium& operator=(WebRtcMedium&&) = delete;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) {
    impl_->CreatePeerConnection(observer, std::move(callback));
  }

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id) {
    return impl_->GetSignalingMessenger(self_id);
  }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::WebRtcMedium> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_WEBRTC_H_
