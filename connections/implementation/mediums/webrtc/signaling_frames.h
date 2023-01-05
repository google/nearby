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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_

#include <vector>

#include "connections/implementation/mediums/webrtc_peer_id.h"
#include "internal/platform/byte_array.h"
#include "proto/mediums/web_rtc_signaling_frames.pb.h"
#include "webrtc/api/peer_connection_interface.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace webrtc_frames {

ByteArray EncodeReadyForSignalingPoke(const WebrtcPeerId& sender_id);

ByteArray EncodeOffer(const WebrtcPeerId& sender_id,
                      const webrtc::SessionDescriptionInterface& offer);
ByteArray EncodeAnswer(const WebrtcPeerId& sender_id,
                       const webrtc::SessionDescriptionInterface& answer);

ByteArray EncodeIceCandidates(
    const WebrtcPeerId& sender_id,
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

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_
