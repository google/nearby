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

#include "core/internal/mediums/webrtc/peer_id.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"
#include "location/nearby/mediums/proto/web_rtc_signaling_frames.pb.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace webrtc_frames {

ConstPtr<ByteArray> EncodeReadyForSignalingPoke(ConstPtr<PeerId> sender_id);

ConstPtr<ByteArray> EncodeOffer(
    ConstPtr<PeerId> sender_id,
    const webrtc::SessionDescriptionInterface& offer);
ConstPtr<ByteArray> EncodeAnswer(
    ConstPtr<PeerId> sender_id,
    const webrtc::SessionDescriptionInterface& answer);

ConstPtr<ByteArray> EncodeIceCandidates(
    ConstPtr<PeerId> sender_id,
    const std::vector<location::nearby::mediums::IceCandidate>& ice_candidates);
location::nearby::mediums::IceCandidate EncodeIceCandidate(
    const webrtc::IceCandidateInterface& ice_candidate);

Ptr<webrtc::SessionDescriptionInterface> DecodeOffer(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);
Ptr<webrtc::SessionDescriptionInterface> DecodeAnswer(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);

std::vector<ConstPtr<webrtc::IceCandidateInterface>> DecodeIceCandidates(
    const location::nearby::mediums::WebRtcSignalingFrame& frame);

}  // namespace webrtc_frames

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SIGNALING_FRAMES_H_
