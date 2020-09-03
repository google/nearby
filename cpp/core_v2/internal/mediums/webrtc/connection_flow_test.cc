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

#include "core_v2/internal/mediums/webrtc/connection_flow.h"

#include <memory>
#include <vector>

#include "core_v2/internal/mediums/webrtc/session_description_wrapper.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/webrtc.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/scoped_refptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

class ConnectionFlowTest : public ::testing::Test {
 protected:
  ConnectionFlowTest() {
    MediumEnvironment::Instance().Stop();
    MediumEnvironment::Instance().Start({.webrtc_enabled = true});
  }
};

std::unique_ptr<webrtc::IceCandidateInterface> CopyCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  return webrtc::CreateIceCandidate(candidate->sdp_mid(),
                                    candidate->sdp_mline_index(),
                                    candidate->candidate());
}

// TODO(bfranz) - Add test that deterministically sends answerer_ice_candidates
// before answer is sent.
TEST_F(ConnectionFlowTest, SuccessfulOfferAnswerFlow) {
  WebRtcMedium webrtc_medium_offerer, webrtc_medium_answerer;

  Future<ByteArray> message_received_future;

  std::unique_ptr<ConnectionFlow> offerer, answerer;

  // Send Ice Candidates immediately when you retrieve them
  offerer = ConnectionFlow::Create(
      {.local_ice_candidate_found_cb =
           [&answerer](const webrtc::IceCandidateInterface* candidate) {
             std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> vec;
             vec.push_back(CopyCandidate(candidate));
             // The callback might be alive while the objects in test are
             // destroyed.
             if (answerer)
               answerer->OnRemoteIceCandidatesReceived(std::move(vec));
           }},
      DataChannelListener(), webrtc_medium_offerer);
  ASSERT_NE(offerer, nullptr);
  answerer = ConnectionFlow::Create(
      {.local_ice_candidate_found_cb =
           [&offerer](const webrtc::IceCandidateInterface* candidate) {
             std::vector<std::unique_ptr<webrtc::IceCandidateInterface>> vec;
             vec.push_back(CopyCandidate(candidate));
             // The callback might be alive while the objects in test are
             // destroyed.
             if (offerer)
               offerer->OnRemoteIceCandidatesReceived(std::move(vec));
           }},
      {.data_channel_message_received_cb =
           [&message_received_future](ByteArray bytes) {
             message_received_future.Set(std::move(bytes));
           }},
      webrtc_medium_answerer);
  ASSERT_NE(answerer, nullptr);

  // Create and send offer
  SessionDescriptionWrapper offer = offerer->CreateOffer();
  EXPECT_EQ(offer.GetType(), webrtc::SdpType::kOffer);
  EXPECT_TRUE(answerer->OnOfferReceived(offer));
  EXPECT_TRUE(offerer->SetLocalSessionDescription(std::move(offer)));

  // Create and send answer
  SessionDescriptionWrapper answer = answerer->CreateAnswer();
  EXPECT_EQ(answer.GetType(), webrtc::SdpType::kAnswer);
  EXPECT_TRUE(offerer->OnAnswerReceived(answer));
  EXPECT_TRUE(answerer->SetLocalSessionDescription(std::move(answer)));

  // Retrieve Data Channels
  ExceptionOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      offerer_channel = offerer->GetDataChannel().Get(absl::Seconds(1));
  EXPECT_TRUE(offerer_channel.ok());
  ExceptionOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      answerer_channel = answerer->GetDataChannel().Get(absl::Seconds(1));
  EXPECT_TRUE(answerer_channel.ok());

  // Send message on data channel
  const char message[] = "Test";
  offerer_channel.result()->Send(webrtc::DataBuffer(message));
  ExceptionOr<ByteArray> received_message =
      message_received_future.Get(absl::Seconds(1));
  EXPECT_TRUE(received_message.ok());
  EXPECT_EQ(received_message.result(), ByteArray{message});
}

TEST_F(ConnectionFlowTest, CreateAnswerBeforeOfferReceived) {
  WebRtcMedium webrtc_medium;

  std::unique_ptr<ConnectionFlow> answerer = ConnectionFlow::Create(
      LocalIceCandidateListener(), DataChannelListener(), webrtc_medium);
  ASSERT_NE(answerer, nullptr);

  SessionDescriptionWrapper answer = answerer->CreateAnswer();
  EXPECT_FALSE(answer.IsValid());
}

TEST_F(ConnectionFlowTest, SetAnswerBeforeOffer) {
  WebRtcMedium webrtc_medium_offerer, webrtc_medium_answerer;

  std::unique_ptr<ConnectionFlow> offerer =
      ConnectionFlow::Create(LocalIceCandidateListener(), DataChannelListener(),
                             webrtc_medium_offerer);
  ASSERT_NE(offerer, nullptr);
  std::unique_ptr<ConnectionFlow> answerer =
      ConnectionFlow::Create(LocalIceCandidateListener(), DataChannelListener(),
                             webrtc_medium_answerer);
  ASSERT_NE(answerer, nullptr);

  SessionDescriptionWrapper offer = offerer->CreateOffer();
  EXPECT_EQ(offer.GetType(), webrtc::SdpType::kOffer);
  // Did not set offer as local session description
  EXPECT_TRUE(answerer->OnOfferReceived(offer));

  SessionDescriptionWrapper answer = answerer->CreateAnswer();
  EXPECT_EQ(answer.GetType(), webrtc::SdpType::kAnswer);
  EXPECT_FALSE(offerer->OnAnswerReceived(answer));
}

TEST_F(ConnectionFlowTest, CannotCreateOfferAfterClose) {
  WebRtcMedium webrtc_medium;

  std::unique_ptr<ConnectionFlow> offerer = ConnectionFlow::Create(
      LocalIceCandidateListener(), DataChannelListener(), webrtc_medium);
  ASSERT_NE(offerer, nullptr);

  EXPECT_TRUE(offerer->Close());

  EXPECT_FALSE(offerer->CreateOffer().IsValid());
}

TEST_F(ConnectionFlowTest, CannotSetSessionDescriptionAfterClose) {
  WebRtcMedium webrtc_medium;

  std::unique_ptr<ConnectionFlow> offerer = ConnectionFlow::Create(
      LocalIceCandidateListener(), DataChannelListener(), webrtc_medium);
  ASSERT_NE(offerer, nullptr);

  SessionDescriptionWrapper offer = offerer->CreateOffer();
  EXPECT_EQ(offer.GetType(), webrtc::SdpType::kOffer);

  EXPECT_TRUE(offerer->Close());

  EXPECT_FALSE(offerer->SetLocalSessionDescription(offer));
}

TEST_F(ConnectionFlowTest, CannotReceiveOfferAfterClose) {
  WebRtcMedium webrtc_medium_offerer, webrtc_medium_answerer;

  std::unique_ptr<ConnectionFlow> offerer =
      ConnectionFlow::Create(LocalIceCandidateListener(), DataChannelListener(),
                             webrtc_medium_offerer);
  ASSERT_NE(offerer, nullptr);
  std::unique_ptr<ConnectionFlow> answerer =
      ConnectionFlow::Create(LocalIceCandidateListener(), DataChannelListener(),
                             webrtc_medium_answerer);
  ASSERT_NE(answerer, nullptr);

  EXPECT_TRUE(answerer->Close());

  SessionDescriptionWrapper offer = offerer->CreateOffer();
  EXPECT_EQ(offer.GetType(), webrtc::SdpType::kOffer);

  EXPECT_FALSE(answerer->OnOfferReceived(offer));
}

TEST_F(ConnectionFlowTest, NullPeerConnection) {
  MediumEnvironment::Instance().SetUseValidPeerConnection(
      /*use_valid_peer_connection=*/false);

  WebRtcMedium medium;
  std::unique_ptr<ConnectionFlow> answerer = ConnectionFlow::Create(
      LocalIceCandidateListener(), DataChannelListener(), medium);
  EXPECT_EQ(answerer, nullptr);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
