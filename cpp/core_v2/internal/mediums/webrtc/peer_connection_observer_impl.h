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

#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_

#include "core_v2/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "platform_v2/public/single_thread_executor.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class ConnectionFlow;

class PeerConnectionObserverImpl : public webrtc::PeerConnectionObserver {
 public:
  ~PeerConnectionObserverImpl() override = default;
  PeerConnectionObserverImpl(
      ConnectionFlow* connection_flow,
      LocalIceCandidateListener local_ice_candidate_listener);

  // webrtc::PeerConnectionObserver:
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
  void OnRenegotiationNeeded() override;

 private:
  void OffloadFromSignalingThread(Runnable runnable);

  ConnectionFlow* connection_flow_;
  LocalIceCandidateListener local_ice_candidate_listener_;
  SingleThreadExecutor single_threaded_signaling_offloader_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_PEER_CONNECTION_OBSERVER_IMPL_H_
