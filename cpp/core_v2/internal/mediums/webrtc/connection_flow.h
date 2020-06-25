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

#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_

#include <memory>

#include "core_v2/internal/mediums/webrtc/data_channel_listener.h"
#include "core_v2/internal/mediums/webrtc/data_channel_observer_impl.h"
#include "core_v2/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "core_v2/internal/mediums/webrtc/peer_connection_observer_impl.h"
#include "core_v2/internal/mediums/webrtc/session_description_wrapper.h"
#include "platform_v2/base/runnable.h"
#include "platform_v2/public/future.h"
#include "platform_v2/public/single_thread_executor.h"
#include "platform_v2/public/webrtc.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

/**
 * Flow for an offerer:
 *
 * <ul>
 *   <li>INITIALIZED: After construction.
 *   <li>CREATING_OFFER: After CreateOffer(). Local ice candidate collection
 * begins.
 *   <li>WAITING_FOR_ANSWER: Until the remote peer sends their answer.
 *   <li>WAITING_TO_CONNECT: Until the data channel actually connects. Remote
 * ice candidates should be added with OnRemoteIceCandidatesReceived as they are
 * gathered.
 *   <li>CONNECTED: We successfully connected to the remote data
 * channel.
 *   <li>ENDED: The final state that can occur from any of the previous
 * states if we disconnect at any point in the flow.
 * </ul>
 *
 * <p>Flow for an answerer:
 *
 * <ul>
 *   <li>INITIALIZED: After construction.
 *   <li>RECEIVED_OFFER: After onOfferReceived().
 *   <li>CREATING_ANSWER: After CreateAnswer(). Local ice candidate collection
 * begins.
 *   <li>WAITING_TO_CONNECT: Until the data channel actually connects.
 * Remote ice candidates should be added with OnRemoteIceCandidatesReceived as
 * they are gathered.
 *   <li>CONNECTED: We successfully connected to the remote
 * data channel.
 *   <li>ENDED: The final state that can occur from any of the
 * previous states if we disconnect at any point in the flow.
 * </ul>
 */
class ConnectionFlow {
 public:
  // This method blocks on the creation of the peer connection object.
  static std::unique_ptr<ConnectionFlow> Create(
      LocalIceCandidateListener local_ice_candidate_listener,
      DataChannelListener data_channel_listener, WebRtcMedium& webrtc_medium);
  ~ConnectionFlow();

  // Create the offer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateOffer.
  SessionDescriptionWrapper CreateOffer() ABSL_LOCKS_EXCLUDED(mutex_);
  // Create the answer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateAnswer.
  SessionDescriptionWrapper CreateAnswer() ABSL_LOCKS_EXCLUDED(mutex_);
  // Set the local session description. |sdp| was created via CreateOffer()
  // or CreateAnswer().
  bool SetLocalSessionDescription(SessionDescriptionWrapper sdp)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an offer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  bool OnOfferReceived(SessionDescriptionWrapper offer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an answer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  bool OnAnswerReceived(SessionDescriptionWrapper answer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an ice candidate was received from a remote; this will add the
  // ice candidate to the peer connection if ready or cache it otherwise.
  bool OnRemoteIceCandidatesReceived(
      std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
          ice_candidates) ABSL_LOCKS_EXCLUDED(mutex_);
  // Get a future for the data channel.
  Future<rtc::scoped_refptr<webrtc::DataChannelInterface>> GetDataChannel();
  // Close the peer connection and data channel.
  bool Close() ABSL_LOCKS_EXCLUDED(mutex_);

  // Invoked when the peer connection indicates that signaling is stable.
  void OnSignalingStable() ABSL_LOCKS_EXCLUDED(mutex_);
  webrtc::DataChannelObserver* CreateDataChannelObserver(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  // Invoked upon changes in the state of peer connection, e.g. react to
  // disconnect.
  void ProcessOnPeerConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
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

  ConnectionFlow(LocalIceCandidateListener local_ice_candidate_listener,
                 DataChannelListener data_channel_listener);

  // TODO(bfranz): Consider whether this needs to be configurable per platform
  static constexpr absl::Duration kTimeout = absl::Milliseconds(250);

  bool InitPeerConnection(WebRtcMedium& webrtc_medium);

  bool TransitionState(State current_state, State new_state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool SetRemoteSessionDescription(SessionDescriptionWrapper sdp);

  void ProcessDataChannelConnected() ABSL_LOCKS_EXCLUDED(mutex_);

  void CloseAndNotifyLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool CloseLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OffloadFromSignalingThread(Runnable runnable);

  Mutex mutex_;

  State state_ ABSL_GUARDED_BY(mutex_) = State::kInitialized;
  DataChannelListener data_channel_listener_;

  std::unique_ptr<DataChannelObserverImpl> data_channel_observer_;

  Future<rtc::scoped_refptr<webrtc::DataChannelInterface>> data_channel_future_;

  PeerConnectionObserverImpl peer_connection_observer_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      cached_remote_ice_candidates_ ABSL_GUARDED_BY(mutex_);

  SingleThreadExecutor single_threaded_signaling_offloader_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
