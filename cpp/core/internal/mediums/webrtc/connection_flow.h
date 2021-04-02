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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_

#include <memory>

#include "core/internal/mediums/webrtc/data_channel_listener.h"
#include "core/internal/mediums/webrtc/data_channel_observer_impl.h"
#include "core/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "core/internal/mediums/webrtc/peer_connection_observer_impl.h"
#include "core/internal/mediums/webrtc/session_description_wrapper.h"
#include "platform/base/runnable.h"
#include "platform/public/logging.h"
#include "platform/public/single_thread_executor.h"
#include "platform/public/webrtc.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class ConnectionFlow {
 public:
  enum class State {
    kInitialized,
    kCreatingOffer,
    kWaitingForAnswer,
    kReceivedOffer,
    kCreatingAnswer,
    kWaitingToConnect,
    kConnected,
    kEnded,
  };

  class CreateSessionDescriptionObserverImpl
      : public webrtc::CreateSessionDescriptionObserver {
   public:
    CreateSessionDescriptionObserverImpl(
        ConnectionFlow* connection_flow,
        Future<SessionDescriptionWrapper> settable_future,
        State new_state_if_successful)
        : connection_flow_(connection_flow),
          settable_future_(std::move(settable_future)),
          new_state_if_successful_(new_state_if_successful) {}
    ~CreateSessionDescriptionObserverImpl() override = default;

    // webrtc::CreateSessionDescriptionObserver
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
      NEARBY_LOG(INFO, "CreateSessionDescriptionObserverImpl::OnSuccess()");
      connection_flow_->TransitionState(connection_flow_->state_,
                                        new_state_if_successful_);
      settable_future_.Set(SessionDescriptionWrapper{desc});
    }

    void OnFailure(webrtc::RTCError error) override {
      NEARBY_LOG(ERROR,
                 "CreateSessionDescriptionObserverImpl::OnFailure(): Error "
                 "when creating session description: %d %s",
                 error.type(), error.message());
      settable_future_.SetException({Exception::kFailed});
    }

   private:
    ConnectionFlow* connection_flow_;
    Future<SessionDescriptionWrapper> settable_future_;
    State new_state_if_successful_;
  };

  // This method blocks on the creation of the peer connection object.
  static std::unique_ptr<ConnectionFlow> Create(
      LocalIceCandidateListener local_ice_candidate_listener,
      DataChannelListener data_channel_listener, WebRtcMedium& webrtc_medium);
  ~ConnectionFlow();

  // Returns the current state of the ConnectionFlow.
  State GetState();

  // Create the offer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateOffer.
  SessionDescriptionWrapper CreateOffer();
  // Create the answer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateAnswer.
  SessionDescriptionWrapper CreateAnswer();
  // Set the local session description. |sdp| was created via CreateOffer()
  // or CreateAnswer().
  bool SetLocalSessionDescription(SessionDescriptionWrapper sdp);
  // Invoked when an offer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  bool OnOfferReceived(SessionDescriptionWrapper offer);
  // Invoked when an answer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  bool OnAnswerReceived(SessionDescriptionWrapper answer);
  // Invoked when an ice candidate was received from a remote; this will add the
  // ice candidate to the peer connection if ready or cache it otherwise.
  bool OnRemoteIceCandidatesReceived(
      std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
          ice_candidates);
  // Close the peer connection and data channel.
  bool Close();

  // Invoked when the peer connection indicates that signaling is stable.
  void OnSignalingStable();
  void RegisterDataChannelObserver(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  // Invoked upon changes in the state of peer connection, e.g. react to
  // disconnect.
  void ProcessOnPeerConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state);

 private:
  ConnectionFlow(LocalIceCandidateListener local_ice_candidate_listener,
                 DataChannelListener data_channel_listener);

  // TODO(bfranz): Consider whether this needs to be configurable per platform
  static constexpr absl::Duration kTimeout = absl::Milliseconds(250);
  static constexpr absl::Duration kPeerConnectionTimeout =
      absl::Milliseconds(2500);

  bool InitPeerConnection(WebRtcMedium& webrtc_medium);
  bool TransitionState(State current_state, State new_state);
  bool SetRemoteSessionDescription(SessionDescriptionWrapper sdp,
                                   State new_state);
  void ProcessDataChannelConnected(
      rtc::scoped_refptr<webrtc::DataChannelInterface>);
  void OffloadFromSignalingThread(Runnable runnable);
  bool RunOnSignalingThread(Runnable runnable);

  State state_ = State::kInitialized;
  DataChannelListener data_channel_listener_;

  std::unique_ptr<DataChannelObserverImpl> data_channel_observer_;

  PeerConnectionObserverImpl peer_connection_observer_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      cached_remote_ice_candidates_;

  SingleThreadExecutor single_threaded_signaling_offloader_;
  rtc::WeakPtrFactory<ConnectionFlow> weak_factory_{this};
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
