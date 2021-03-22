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

#include "core/internal/mediums/webrtc/peer_connection_observer_impl.h"

#include "core/internal/mediums/webrtc/connection_flow.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

PeerConnectionObserverImpl::PeerConnectionObserverImpl(
    ConnectionFlow* connection_flow,
    LocalIceCandidateListener local_ice_candidate_listener)
    : connection_flow_(connection_flow),
      local_ice_candidate_listener_(std::move(local_ice_candidate_listener)) {}

PeerConnectionObserverImpl::~PeerConnectionObserverImpl() {
  {
    MutexLock lock(&mutex_);
    connection_flow_ = nullptr;
  }
  single_threaded_signaling_offloader_.Shutdown();
}

void PeerConnectionObserverImpl::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  local_ice_candidate_listener_.local_ice_candidate_found_cb(candidate);
}

void PeerConnectionObserverImpl::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  NEARBY_LOG(INFO, "OnSignalingChange: %d", new_state);

  OffloadFromSignalingThread([this, new_state]() {
    MutexLock lock(&mutex_);
    if (new_state == webrtc::PeerConnectionInterface::SignalingState::kStable &&
        connection_flow_) {
      connection_flow_->OnSignalingStable();
    }
  });
}

void PeerConnectionObserverImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  NEARBY_LOG(INFO, "OnDataChannel");

  webrtc::DataChannelObserver* data_channel_observer = nullptr;
  {
    MutexLock lock(&mutex_);
    if (!connection_flow_) {
      return;
    }

    data_channel_observer =
        connection_flow_->CreateDataChannelObserver(data_channel);
  }
  data_channel->RegisterObserver(data_channel_observer);
}

void PeerConnectionObserverImpl::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  NEARBY_LOG(INFO, "OnIceGatheringChange: %d", new_state);
}

void PeerConnectionObserverImpl::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  NEARBY_LOG(INFO, "OnConnectionChange: %d", new_state);
  // Only callback into the connection_flow_ if we are not already disconnected.
  MutexLock lock(&mutex_);
  if (connection_flow_) {
    OffloadFromSignalingThread([this, new_state]() {
      MutexLock lock(&mutex_);
      if (connection_flow_) {
        connection_flow_->ProcessOnPeerConnectionChange(new_state);
      }
    });
  }
}

void PeerConnectionObserverImpl ::OnRenegotiationNeeded() {
  NEARBY_LOG(INFO, "OnRenegotiationNeeded");
}

void PeerConnectionObserverImpl::DisconnectConnectionFlow() {
  MutexLock lock(&mutex_);
  connection_flow_ = nullptr;
}

void PeerConnectionObserverImpl::OffloadFromSignalingThread(Runnable runnable) {
  single_threaded_signaling_offloader_.Execute(std::move(runnable));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
