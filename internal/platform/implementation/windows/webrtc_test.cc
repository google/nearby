// Copyright 2023 Google LLC
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

#include "internal/platform/implementation/windows/webrtc.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace windows {

class MockPeerConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {}

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
  }
};

location::nearby::connections::LocationHint GetCountryCodeLocationHint(
    const std::string& country_code) {
  auto location_hint = location::nearby::connections::LocationHint();
  location_hint.set_location(country_code);
  location_hint.set_format(
      location::nearby::connections::LocationStandard::ISO_3166_1_ALPHA_2);
  return location_hint;
}

TEST(WebrtcTest, CountryCodeDefault) {
  WebRtcMedium medium;
  std::string result = medium.GetDefaultCountryCode();
  EXPECT_EQ(result, "US");
}

TEST(WebrtcTest, CreatePeerConnectionSucceeds) {
  auto observer = std::make_unique<MockPeerConnectionObserver>();
  WebRtcMedium medium;
  medium.CreatePeerConnection(
      observer.get(), [](rtc::scoped_refptr<webrtc::PeerConnectionInterface>
                             peer_connection) mutable {
        if (!peer_connection) {
          FAIL() << "Peer connection should have been non-null";
          return;
        }
      });
}

TEST(WebrtcTest, GetSignalingMessengerSucceeds) {
  WebRtcMedium medium;
  std::unique_ptr<api::WebRtcSignalingMessenger> messenger =
      medium.GetSignalingMessenger("US", GetCountryCodeLocationHint("US"));
  EXPECT_TRUE(messenger);
}

}  // namespace windows
}  // namespace nearby
