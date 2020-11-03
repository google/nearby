#ifndef PLATFORM_IMPL_G3_WEBRTC_H_
#define PLATFORM_IMPL_G3_WEBRTC_H_

#include <memory>

#include "platform/api/webrtc.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace g3 {

class WebRtcSignalingMessenger : public api::WebRtcSignalingMessenger {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;

  explicit WebRtcSignalingMessenger(
      absl::string_view self_id,
      const connections::LocationHint& location_hint);
  ~WebRtcSignalingMessenger() override = default;

  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override;
  bool StartReceivingMessages(OnSignalingMessageCallback listener) override;
  void StopReceivingMessages() override;

 private:
  absl::string_view self_id_;
  connections::LocationHint location_hint_;
};

class WebRtcMedium : public api::WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium() = default;
  ~WebRtcMedium() override = default;

  const std::string GetDefaultCountryCode() override;

  // Creates and returns a new webrtc::PeerConnectionInterface object via
  // |callback|.
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override;

  // Returns a signaling messenger for sending WebRTC signaling messages.
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      absl::string_view self_id,
      const connections::LocationHint& location_hint) override;

 private:
  std::unique_ptr<rtc::Thread> signaling_thread_;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_WEBRTC_H_
