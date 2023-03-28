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

#ifndef NO_WEBRTC

#include <memory>

#include "connections/implementation/mediums/webrtc/data_channel_listener.h"
#include "connections/implementation/mediums/webrtc/local_ice_candidate_listener.h"
#include "connections/implementation/mediums/webrtc/session_description_wrapper.h"
#include "connections/implementation/mediums/webrtc_socket.h"
#include "internal/platform/runnable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/webrtc.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"

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
class ConnectionFlow : public webrtc::PeerConnectionObserver {
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

  // This method blocks on the creation of the peer connection object.
  // Can be called on any thread but never called on signaling thread.
  static std::unique_ptr<ConnectionFlow> Create(
      LocalIceCandidateListener local_ice_candidate_listener,
      DataChannelListener data_channel_listener, WebRtcMedium& webrtc_medium);
  ~ConnectionFlow() override;

  // Create the offer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateOffer.
  // Can be called on any thread but never called on signaling thread.
  SessionDescriptionWrapper CreateOffer() ABSL_LOCKS_EXCLUDED(mutex_);
  // Create the answer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateAnswer.
  // Can be called on any thread but never called on signaling thread.
  SessionDescriptionWrapper CreateAnswer() ABSL_LOCKS_EXCLUDED(mutex_);
  // Set the local session description. |sdp| was created via CreateOffer()
  // or CreateAnswer().
  // Can be called on any thread but never called on signaling thread.
  bool SetLocalSessionDescription(SessionDescriptionWrapper sdp)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an offer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  // Can be called on any thread but never called on signaling thread.
  bool OnOfferReceived(SessionDescriptionWrapper offer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an answer was received from a remote; this will set the remote
  // session description on the peer connection. Returns true if the offer was
  // successfully set as remote session description.
  // Can be called on any thread but never called on signaling thread.
  bool OnAnswerReceived(SessionDescriptionWrapper answer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an ice candidate was received from a remote; this will add the
  // ice candidate to the peer connection if ready or cache it otherwise.
  // Can be called on any thread but never called on signaling thread.
  bool OnRemoteIceCandidatesReceived(
      std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
          ice_candidates) ABSL_LOCKS_EXCLUDED(mutex_);
  // Close the peer connection and data channel if not connected.
  // Can be called on any thread but never called on signaling thread.
  bool CloseIfNotConnected() ABSL_LOCKS_EXCLUDED(mutex_);

  // webrtc::PeerConnectionObserver:
  // All methods called only on signaling thread.
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

  // Public because it's used in tests too.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeerConnection();

 private:
  ConnectionFlow(LocalIceCandidateListener local_ice_candidate_listener,
                 DataChannelListener data_channel_listener);

  // Resets peer connection reference. Returns old value.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface>
  GetAndResetPeerConnection();
  void CreateOfferOnSignalingThread(
      Future<SessionDescriptionWrapper> success_future);
  void CreateAnswerOnSignalingThread(
      Future<SessionDescriptionWrapper> success_future);
  void AddIceCandidatesOnSignalingThread(
      std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
          ice_candidates);
  // Invoked when the peer connection indicates that signaling is stable.
  void OnSignalingStable() ABSL_LOCKS_EXCLUDED(mutex_);

  void CreateSocketFromDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  // TODO(bfranz): Consider whether this needs to be configurable per platform
  static constexpr absl::Duration kTimeout = absl::Milliseconds(250);
  static constexpr absl::Duration kPeerConnectionTimeout =
      absl::Milliseconds(2500);

  bool InitPeerConnection(WebRtcMedium& webrtc_medium);

  bool TransitionState(State current_state, State new_state);

  bool SetRemoteSessionDescription(SessionDescriptionWrapper sdp,
                                   State expected_entry_state,
                                   State exit_state);

  bool CloseOnSignalingThread() ABSL_LOCKS_EXCLUDED(mutex_);

  bool RunOnSignalingThread(Runnable&& runnable);
  bool IsRunningOnSignalingThread();

  Mutex mutex_;
  // Used to prevent the destructor from returning while the signaling thread is
  // still running CloseOnSignalingThread()
  CountDownLatch shutdown_latch_{1};

  // State is used on signaling thread only.
  State state_ = State::kInitialized;
  // Used to communicate data channel events back to the caller of Create()
  DataChannelListener data_channel_listener_;

  LocalIceCandidateListener local_ice_candidate_listener_;
  // Peer connection can be used only on signaling thread. The only exception
  // is accessing the signaling thread handle. Tasks posted on the
  // signaling thread may outlive both |peer_connection_| and |this| objects.
  // A mutex is required to access peer connection reference because peer
  // connection object and the reference can be initialized on different
  // threads - the reference could be initialized before peer connection's
  // constructor has finished.
  // |peer_connection_| is actually implemented by PeerConnectionProxy, which
  // runs the real PeerConnection's methods on the correct thread (signaling or
  // worker). If a proxy method is called on the correct thread, then the real
  // method is called directly. Otherwise, a task is posted on the correct
  // thread and the current thread is blocked until that task finishes. We
  // choose to explicitly use |peer_connection_| on the signaling thread,
  // because it allows us to do state management on the signaling thread too,
  // simplifies locking, and we don't have to block the current thread for every
  // peer connection call.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_
      ABSL_GUARDED_BY(mutex_);

  // Used to hold a reference to the WebRtcSocket while the data channel is
  // connecting.
  WebRtcSocketWrapper socket_wrapper_;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      cached_remote_ice_candidates_;
  // This pointer is only for DCHECK() assertions.
  // It allows us to check if we are running on signaling thread even
  // after destroying |peer_connection_|.
  const void* signaling_thread_for_dcheck_only_ = nullptr;
  // This shared_ptr is reset on the signaling thread when ConnectionFlow is
  // closed. This prevents us from running tasks on the signaling thread when
  // peer connection is closed. The value stored in |can_run_tasks_| is not
  // used. We are using std::shared_ptr instead of rtc::WeakPtrFactory because
  // the former is thread-safe.
  std::shared_ptr<void> can_run_tasks_ = std::make_shared<int>();

  friend class CreateSessionDescriptionObserverImpl;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
