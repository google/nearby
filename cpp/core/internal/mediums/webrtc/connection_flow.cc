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

#include "core/internal/mediums/webrtc/connection_flow.h"

#include <iterator>
#include <memory>

#include "core/internal/mediums/webrtc/session_description_wrapper.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"
#include "platform/public/webrtc.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

constexpr absl::Duration ConnectionFlow::kTimeout;
constexpr absl::Duration ConnectionFlow::kPeerConnectionTimeout;

namespace {
// This is the same as the nearby data channel name.
const char kDataChannelName[] = "dataChannel";

class CreateSessionDescriptionObserverImpl
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionObserverImpl(
      Future<SessionDescriptionWrapper>* settable_future)
      : settable_future_(settable_future) {}
  ~CreateSessionDescriptionObserverImpl() override = default;

  // webrtc::CreateSessionDescriptionObserver
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    settable_future_->Set(SessionDescriptionWrapper{desc});
  }

  void OnFailure(webrtc::RTCError error) override {
    NEARBY_LOG(ERROR, "Error when creating session description: %s",
               error.message());
    settable_future_->SetException({Exception::kFailed});
  }

 private:
  std::unique_ptr<Future<SessionDescriptionWrapper>> settable_future_;
};

class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  explicit SetLocalDescriptionObserver(Future<bool>* settable_future)
      : settable_future_(settable_future) {}

  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    // On success, |error.ok()| is true.
    if (error.ok()) {
      settable_future_->Set(true);
      return;
    }

    NEARBY_LOG(ERROR, "Error when setting local session description: %s",
               error.message());
    settable_future_->SetException({Exception::kFailed});
  }
 private:
  std::unique_ptr<Future<bool>> settable_future_;
};

class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  explicit SetRemoteDescriptionObserver(Future<bool>* settable_future)
      : settable_future_(settable_future) {}

  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    // On success, |error.ok()| is true.
    if (error.ok()) {
      settable_future_->Set(true);
      return;
    }

    NEARBY_LOG(ERROR, "Error when setting remote session description: %s",
               error.message());
    settable_future_->SetException({Exception::kFailed});
  }
 private:
  std::unique_ptr<Future<bool>> settable_future_;
};

using PeerConnectionState =
    webrtc::PeerConnectionInterface::PeerConnectionState;

}  // namespace

std::unique_ptr<ConnectionFlow> ConnectionFlow::Create(
    LocalIceCandidateListener local_ice_candidate_listener,
    DataChannelListener data_channel_listener, WebRtcMedium& webrtc_medium) {
  auto connection_flow = absl::WrapUnique(
      new ConnectionFlow(std::move(local_ice_candidate_listener),
                         std::move(data_channel_listener)));
  if (connection_flow->InitPeerConnection(webrtc_medium)) {
    return connection_flow;
  }

  return nullptr;
}

ConnectionFlow::ConnectionFlow(
    LocalIceCandidateListener local_ice_candidate_listener,
    DataChannelListener data_channel_listener)
    : data_channel_listener_(std::move(data_channel_listener)),
      local_ice_candidate_listener_(std::move(local_ice_candidate_listener)) {}

ConnectionFlow::~ConnectionFlow() { Close(); }

ConnectionFlow::State ConnectionFlow::GetState() {
  MutexLock lock(&mutex_);
  return state_;
}

SessionDescriptionWrapper ConnectionFlow::CreateOffer() {
  MutexLock lock(&mutex_);

  if (!TransitionState(State::kInitialized, State::kCreatingOffer)) {
    return SessionDescriptionWrapper();
  }

  webrtc::DataChannelInit data_channel_init;
  data_channel_init.reliable = true;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel =
      peer_connection_->CreateDataChannel(kDataChannelName, &data_channel_init);
  RegisterDataChannelObserver(data_channel);

  auto success_future = new Future<SessionDescriptionWrapper>();
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer =
      new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
          success_future);
  peer_connection_->CreateOffer(observer, options);

  ExceptionOr<SessionDescriptionWrapper> result = success_future->Get(kTimeout);
  if (result.ok() &&
      TransitionState(State::kCreatingOffer, State::kWaitingForAnswer)) {
    return std::move(result.result());
  }

  NEARBY_LOG(ERROR, "Failed to create offer: %d", result.exception());
  return SessionDescriptionWrapper();
}

SessionDescriptionWrapper ConnectionFlow::CreateAnswer() {
  MutexLock lock(&mutex_);

  if (!TransitionState(State::kReceivedOffer, State::kCreatingAnswer)) {
    return SessionDescriptionWrapper();
  }

  auto success_future = new Future<SessionDescriptionWrapper>();
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer =
      new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
          success_future);
  peer_connection_->CreateAnswer(observer, options);

  ExceptionOr<SessionDescriptionWrapper> result = success_future->Get(kTimeout);
  if (result.ok() &&
      TransitionState(State::kCreatingAnswer, State::kWaitingToConnect)) {
    return std::move(result.result());
  }

  NEARBY_LOG(ERROR, "Failed to create answer: %d", result.exception());
  return SessionDescriptionWrapper();
}

bool ConnectionFlow::SetLocalSessionDescription(SessionDescriptionWrapper sdp) {
  MutexLock lock(&mutex_);

  if (state_ == State::kEnded) return false;
  if (!sdp.IsValid()) return false;

  auto success_future = new Future<bool>();
  rtc::scoped_refptr<SetLocalDescriptionObserver> observer =
      new rtc::RefCountedObject<SetLocalDescriptionObserver>(
          success_future);

  peer_connection_->SetLocalDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
      observer);

  ExceptionOr<bool> result = success_future->Get(kTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set local session description: %d",
               result.exception());
  }
  return success;
}

bool ConnectionFlow::SetRemoteSessionDescription(
    SessionDescriptionWrapper sdp) {
  if (!sdp.IsValid()) return false;

  auto success_future = new Future<bool>();
  rtc::scoped_refptr<SetRemoteDescriptionObserver> observer =
      new rtc::RefCountedObject<SetRemoteDescriptionObserver>(
          success_future);

  peer_connection_->SetRemoteDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
      observer);

  ExceptionOr<bool> result = success_future->Get(kTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set remote description: %d",
               result.exception());
  }
  return success;
}

bool ConnectionFlow::OnOfferReceived(SessionDescriptionWrapper offer) {
  MutexLock lock(&mutex_);

  if (!TransitionState(State::kInitialized, State::kReceivedOffer)) {
    return false;
  }
  return SetRemoteSessionDescription(std::move(offer));
}

bool ConnectionFlow::OnAnswerReceived(SessionDescriptionWrapper answer) {
  MutexLock lock(&mutex_);

  if (!TransitionState(State::kWaitingForAnswer, State::kWaitingToConnect)) {
    return false;
  }
  return SetRemoteSessionDescription(std::move(answer));
}

bool ConnectionFlow::OnRemoteIceCandidatesReceived(
    std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
        ice_candidates) {
  MutexLock lock(&mutex_);

  if (state_ == State::kEnded) {
    NEARBY_LOG(WARNING,
               "You cannot add ice candidates to a disconnected session.");
    return false;
  }

  if (state_ != State::kWaitingToConnect && state_ != State::kConnected) {
    cached_remote_ice_candidates_.insert(
        cached_remote_ice_candidates_.end(),
        std::make_move_iterator(ice_candidates.begin()),
        std::make_move_iterator(ice_candidates.end()));
    return true;
  }

  for (auto&& ice_candidate : ice_candidates) {
    if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
      NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
    }
  }
  return true;
}

bool ConnectionFlow::Close() {
  return Close(/* close_peer_connection= */ true);
}

bool ConnectionFlow::InitPeerConnection(WebRtcMedium& webrtc_medium) {
  Future<bool> success_future;
  // CreatePeerConnection callback may be invoked after ConnectionFlow lifetime
  // has ended, in case of a timeout. Future is captured by value, and is safe
  // to access, but it is not safe to access ConnectionFlow member variables
  // unless the Future::Set() returns true.
  webrtc_medium.CreatePeerConnection(
      this,
      [this, success_future](rtc::scoped_refptr<webrtc::PeerConnectionInterface>
                                 peer_connection) mutable {
        if (!peer_connection) {
          success_future.Set(false);
          return;
        }

        // If this fails, means we have already assigned something to
        // success_future; it is either:
        // 1) this is the 2nd call of this callback (and this is a bug), or
        // 2) Get(timeout) has set the future value as exception already.
        if (success_future.IsSet()) return;
        peer_connection_ = peer_connection;
        success_future.Set(true);
      });

  ExceptionOr<bool> result = success_future.Get(kPeerConnectionTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to create peer connection: %d",
               result.exception());
  }
  return success;
}

void ConnectionFlow::OnSignalingStable() {
  OffloadFromSignalingThread([this] {
    MutexLock lock(&mutex_);

    if (state_ != State::kWaitingToConnect && state_ != State::kConnected)
      return;

    for (auto&& ice_candidate : cached_remote_ice_candidates_) {
      if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
        NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
      }
    }
    cached_remote_ice_candidates_.clear();
  });
}

void ConnectionFlow::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  local_ice_candidate_listener_.local_ice_candidate_found_cb(candidate);
}

void ConnectionFlow::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  NEARBY_LOG(INFO, "OnSignalingChange: %d", new_state);
  if (new_state == webrtc::PeerConnectionInterface::SignalingState::kStable) {
    OnSignalingStable();
  }
}

void ConnectionFlow::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  NEARBY_LOG(INFO, "OnDataChannel");
  RegisterDataChannelObserver(std::move(data_channel));
}

void ConnectionFlow::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  NEARBY_LOG(INFO, "OnIceGatheringChange: %d", new_state);
}

void ConnectionFlow::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  NEARBY_LOG(INFO, "OnConnectionChange: %d", new_state);
  if (new_state == PeerConnectionState::kClosed ||
      new_state == PeerConnectionState::kFailed ||
      new_state == PeerConnectionState::kDisconnected) {
    // kClosed means that PeerConnection is already closed or
    // is closing right now - PeerConnection::Close() triggered
    // PeerConnectionObserver::OnConnectionChange(kClosed).
    // We must not call PeerConnection::Close() again on that code path
    Close(new_state != PeerConnectionState::kClosed);
  }
}

void ConnectionFlow::OnRenegotiationNeeded() {
  NEARBY_LOG(INFO, "OnRenegotiationNeeded");
}

void ConnectionFlow::ProcessDataChannelConnected(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  MutexLock lock(&mutex_);
  NEARBY_LOG(INFO, "Data channel state changed to connected.");
  if (!TransitionState(State::kWaitingToConnect, State::kConnected)) {
    data_channel->Close();
    return;
  }

  data_channel_listener_.data_channel_created_cb(std::move(data_channel));
}

void ConnectionFlow::RegisterDataChannelObserver(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (!data_channel_observer_) {
    auto state_change_callback = [this, data_channel]() {
      if (data_channel->state() ==
          webrtc::DataChannelInterface::DataState::kOpen) {
        OffloadFromSignalingThread(
            [this, data_channel{std::move(data_channel)}]() {
              ProcessDataChannelConnected(std::move(data_channel));
            });
      }
    };
    data_channel_observer_ = absl::make_unique<DataChannelObserverImpl>(
        data_channel, &data_channel_listener_,
        std::move(state_change_callback));
    NEARBY_LOG(INFO, "Registered data channel observer");
  } else {
    NEARBY_LOG(WARNING, "Data channel observer already exists");
  }
}

bool ConnectionFlow::TransitionState(State current_state, State new_state) {
  if (current_state != state_) {
    NEARBY_LOG(
        WARNING,
        "Invalid state transition to %d: current state is %d but expected %d.",
        new_state, state_, current_state);
    return false;
  }
  state_ = new_state;
  return true;
}

bool ConnectionFlow::Close(bool close_peer_connection) {
  {
    MutexLock lock(&mutex_);
    if (state_ == State::kEnded) {
      return false;
    }
    state_ = State::kEnded;
  }
  NEARBY_LOG(INFO, "Closing WebRTC connection.");

  single_threaded_signaling_offloader_.Shutdown();

  if (peer_connection_ && close_peer_connection) peer_connection_->Close();

  data_channel_observer_.reset();
  NEARBY_LOG(INFO, "Closed WebRTC connection.");
  return true;
}

void ConnectionFlow::OffloadFromSignalingThread(Runnable runnable) {
  single_threaded_signaling_offloader_.Execute(std::move(runnable));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
