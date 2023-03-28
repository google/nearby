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

#ifndef NO_WEBRTC

#include "connections/implementation/mediums/webrtc/signaling_frames.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace webrtc_frames {
using WebRtcSignalingFrame = ::location::nearby::mediums::WebRtcSignalingFrame;

namespace {

ByteArray FrameToByteArray(const WebRtcSignalingFrame& signaling_frame) {
  std::string message;
  signaling_frame.SerializeToString(&message);
  return ByteArray(message.c_str(), message.size());
}

void SetSenderId(const WebrtcPeerId& sender_id, WebRtcSignalingFrame& frame) {
  frame.mutable_sender_id()->set_id(sender_id.GetId());
}

std::unique_ptr<webrtc::IceCandidateInterface> DecodeIceCandidate(
    location::nearby::mediums::IceCandidate ice_candidate_proto) {
  webrtc::SdpParseError error;
  return std::unique_ptr<webrtc::IceCandidateInterface>(
      webrtc::CreateIceCandidate(ice_candidate_proto.sdp_mid(),
                                 ice_candidate_proto.sdp_m_line_index(),
                                 ice_candidate_proto.sdp(), &error));
}

}  // namespace

ByteArray EncodeReadyForSignalingPoke(const WebrtcPeerId& sender_id) {
  WebRtcSignalingFrame signaling_frame;
  signaling_frame.set_type(WebRtcSignalingFrame::READY_FOR_SIGNALING_POKE_TYPE);
  SetSenderId(sender_id, signaling_frame);
  signaling_frame.set_allocated_ready_for_signaling_poke(
      new location::nearby::mediums::ReadyForSignalingPoke());
  return FrameToByteArray(std::move(signaling_frame));
}

ByteArray EncodeOffer(const WebrtcPeerId& sender_id,
                      const webrtc::SessionDescriptionInterface& offer) {
  WebRtcSignalingFrame signaling_frame;
  signaling_frame.set_type(WebRtcSignalingFrame::OFFER_TYPE);
  SetSenderId(sender_id, signaling_frame);
  std::string offer_str;
  offer.ToString(&offer_str);
  signaling_frame.mutable_offer()
      ->mutable_session_description()
      ->set_description(offer_str);
  return FrameToByteArray(std::move(signaling_frame));
}

ByteArray EncodeAnswer(const WebrtcPeerId& sender_id,
                       const webrtc::SessionDescriptionInterface& answer) {
  WebRtcSignalingFrame signaling_frame;
  signaling_frame.set_type(WebRtcSignalingFrame::ANSWER_TYPE);
  SetSenderId(sender_id, signaling_frame);
  std::string answer_str;
  answer.ToString(&answer_str);
  signaling_frame.mutable_answer()
      ->mutable_session_description()
      ->set_description(answer_str);
  return FrameToByteArray(std::move(signaling_frame));
}

ByteArray EncodeIceCandidates(
    const WebrtcPeerId& sender_id,
    const std::vector<location::nearby::mediums::IceCandidate>&
        ice_candidates) {
  WebRtcSignalingFrame signaling_frame;
  signaling_frame.set_type(WebRtcSignalingFrame::ICE_CANDIDATES_TYPE);
  SetSenderId(sender_id, signaling_frame);
  for (const auto& ice_candidate : ice_candidates) {
    *signaling_frame.mutable_ice_candidates()->add_ice_candidates() =
        ice_candidate;
  }
  return FrameToByteArray(std::move(signaling_frame));
}

std::unique_ptr<webrtc::SessionDescriptionInterface> DecodeOffer(
    const WebRtcSignalingFrame& frame) {
  return webrtc::CreateSessionDescription(
      webrtc::SdpType::kOffer,
      frame.offer().session_description().description());
}

std::unique_ptr<webrtc::SessionDescriptionInterface> DecodeAnswer(
    const WebRtcSignalingFrame& frame) {
  return webrtc::CreateSessionDescription(
      webrtc::SdpType::kAnswer,
      frame.answer().session_description().description());
}

std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> DecodeIceCandidates(
    const WebRtcSignalingFrame& frame) {
  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> ice_candidates;
  for (const auto& candidate : frame.ice_candidates().ice_candidates()) {
    ice_candidates.push_back(DecodeIceCandidate(candidate));
  }
  return ice_candidates;
}

location::nearby::mediums::IceCandidate EncodeIceCandidate(
    const webrtc::IceCandidateInterface& ice_candidate) {
  std::string sdp;
  ice_candidate.ToString(&sdp);
  location::nearby::mediums::IceCandidate ice_candidate_proto;
  ice_candidate_proto.set_sdp(sdp);
  ice_candidate_proto.set_sdp_mid(ice_candidate.sdp_mid());
  ice_candidate_proto.set_sdp_m_line_index(ice_candidate.sdp_mline_index());
  return ice_candidate_proto;
}

}  // namespace webrtc_frames
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif
