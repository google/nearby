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

#include "connections/implementation/mediums/webrtc/signaling_frames.h"

#include <memory>

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/mediums/webrtc_peer_id.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace webrtc_frames {

namespace {

using ::location::nearby::mediums::IceCandidate;
using ::location::nearby::mediums::WebRtcSignalingFrame;
const char kSampleSdp[] =
    "v=0\r\no=- 7859371131 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 "
    "0\r\na=msid-semantic: WMS\r\n";

const char kIceCandidateSdp1[] =
    "a=candidate:1 1 UDP 2130706431 10.0.1.1 8998 typ host";
const char kIceCandidateSdp2[] =
    "a=candidate:2 1 UDP 1694498815 192.0.2.3 45664 typ srflx raddr";

const char kIceSdpMid[] = "data";
const int kIceSdpMLineIndex = 0;

const char kOfferProto[] = R"(
        sender_id { id: "abc" }
        type: OFFER_TYPE
        offer {
          session_description {
            description: "v=0\r\no=- 7859371131 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\n"
          }
        }
      )";

const char kAnswerProto[] = R"(
        sender_id { id: "abc" }
        type: ANSWER_TYPE
        answer {
          session_description {
            description: "v=0\r\no=- 7859371131 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=msid-semantic: WMS\r\n"
          }
        }
      )";

const char kIceCandidatesProto[] = R"(
        sender_id { id: "abc" }
        type: ICE_CANDIDATES_TYPE
        ice_candidates {
          ice_candidates {
            sdp: "candidate:1 1 udp 2130706431 10.0.1.1 8998 typ host generation 0"
            sdp_mid: "data"
            sdp_m_line_index: 0
          }
          ice_candidates {
            sdp: "candidate:2 1 udp 1694498815 192.0.2.3 45664 typ srflx generation 0"
            sdp_mid: "data"
            sdp_m_line_index: 0
          }
        }
      )";
}  // namespace

TEST(SignalingFramesTest, SignalingPoke) {
  WebrtcPeerId sender_id("abc");
  ByteArray encoded_poke = EncodeReadyForSignalingPoke(sender_id);

  WebRtcSignalingFrame frame;
  frame.ParseFromString(std::string(encoded_poke.data(), encoded_poke.size()));

  EXPECT_THAT(frame, protobuf_matchers::EqualsProto(R"pb(
                sender_id { id: "abc" }
                type: READY_FOR_SIGNALING_POKE_TYPE
                ready_for_signaling_poke {}
              )pb"));
}

TEST(SignalingFramesTest, EncodeValidOffer) {
  WebrtcPeerId sender_id("abc");
  std::unique_ptr<webrtc::SessionDescriptionInterface> offer =
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, kSampleSdp);
  ByteArray encoded_offer = EncodeOffer(sender_id, *offer);

  WebRtcSignalingFrame frame;
  frame.ParseFromString(
      std::string(encoded_offer.data(), encoded_offer.size()));

  EXPECT_THAT(frame, protobuf_matchers::EqualsProto(kOfferProto));
}

TEST(SignaingFramesTest, DecodeValidOffer) {
  WebRtcSignalingFrame frame;
  proto2::TextFormat::ParseFromString(kOfferProto, &frame);
  std::unique_ptr<webrtc::SessionDescriptionInterface> decoded_offer =
      DecodeOffer(frame);

  EXPECT_EQ(webrtc::SdpType::kOffer, decoded_offer->GetType());
  std::string description;
  decoded_offer->ToString(&description);
  EXPECT_EQ(kSampleSdp, description);
}

TEST(SignalingFramesTest, EncodeValidAnswer) {
  WebrtcPeerId sender_id("abc");
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, kSampleSdp));
  ByteArray encoded_answer = EncodeAnswer(sender_id, *answer);

  WebRtcSignalingFrame frame;
  frame.ParseFromString(
      std::string(encoded_answer.data(), encoded_answer.size()));

  EXPECT_THAT(frame, protobuf_matchers::EqualsProto(kAnswerProto));
}

TEST(SignalingFramesTest, DecodeValidAnswer) {
  WebRtcSignalingFrame frame;
  proto2::TextFormat::ParseFromString(kAnswerProto, &frame);
  std::unique_ptr<webrtc::SessionDescriptionInterface> decoded_answer =
      DecodeAnswer(frame);

  EXPECT_EQ(webrtc::SdpType::kAnswer, decoded_answer->GetType());
  std::string description;
  decoded_answer->ToString(&description);
  EXPECT_EQ(kSampleSdp, description);
}

TEST(SignalingFramesTest, EncodeValidIceCandidates) {
  WebrtcPeerId sender_id("abc");
  webrtc::SdpParseError error;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> ice_candidates;
  ice_candidates.emplace_back(webrtc::CreateIceCandidate(
      kIceSdpMid, kIceSdpMLineIndex, kIceCandidateSdp1, &error));
  ice_candidates.emplace_back(webrtc::CreateIceCandidate(
      kIceSdpMid, kIceSdpMLineIndex, kIceCandidateSdp2, &error));
  std::vector<IceCandidate> encoded_candidates_vec;
  for (const auto& ice_candidate : ice_candidates) {
    encoded_candidates_vec.push_back(EncodeIceCandidate(*ice_candidate));
  }
  ByteArray encoded_candidates =
      EncodeIceCandidates(sender_id, encoded_candidates_vec);

  WebRtcSignalingFrame frame;
  frame.ParseFromString(
      std::string(encoded_candidates.data(), encoded_candidates.size()));

  EXPECT_THAT(frame, protobuf_matchers::EqualsProto(kIceCandidatesProto));
}

TEST(SignalingFramesTest, DecodeValidIceCandidates) {
  webrtc::SdpParseError error;
  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> ice_candidates;
  ice_candidates.emplace_back(webrtc::CreateIceCandidate(
      kIceSdpMid, kIceSdpMLineIndex, kIceCandidateSdp1, &error));
  ice_candidates.emplace_back(webrtc::CreateIceCandidate(
      kIceSdpMid, kIceSdpMLineIndex, kIceCandidateSdp2, &error));

  WebRtcSignalingFrame frame;
  proto2::TextFormat::ParseFromString(kIceCandidatesProto, &frame);
  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      decoded_candidates = DecodeIceCandidates(frame);

  ASSERT_EQ(2u, decoded_candidates.size());
  for (int i = 0; i < static_cast<int>(decoded_candidates.size()); i++) {
    EXPECT_TRUE(ice_candidates[i]->candidate().IsEquivalent(
        decoded_candidates[i]->candidate()));
    EXPECT_EQ(ice_candidates[i]->sdp_mid(), decoded_candidates[i]->sdp_mid());
    EXPECT_EQ(ice_candidates[i]->sdp_mline_index(),
              decoded_candidates[i]->sdp_mline_index());
  }
}

}  // namespace webrtc_frames
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
