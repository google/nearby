#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_

#include <vector>

#include "core/internal/mediums/webrtc/peer_id.h"
#include "platform/base/byte_array.h"
#include "location/nearby/mediums/proto/web_rtc_signaling_frames.pb.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace webrtc_frames {

ByteArray EncodeReadyForSignalingPoke(const PeerId& sender_id);

ByteArray EncodeOffer(const PeerId& sender_id,
                      const webrtc::SessionDescriptionInterface& offer);
ByteArray EncodeAnswer(const PeerId& sender_id,
                       const webrtc::SessionDescriptionInterface& answer);

ByteArray EncodeIceCandidates(
    const PeerId& sender_id,
    const std::vector<location::nearby::mediums::IceCandidate>& ice_candidates);
location::nearby::mediums::IceCandidate EncodeIceCandidate(
    const webrtc::IceCandidateInterface& ice_candidate);

std::unique_ptr<webrtc::SessionDescriptionInterface> DecodeOffer(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);
std::unique_ptr<webrtc::SessionDescriptionInterface> DecodeAnswer(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);

std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> DecodeIceCandidates(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);

}  // namespace webrtc_frames
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_
