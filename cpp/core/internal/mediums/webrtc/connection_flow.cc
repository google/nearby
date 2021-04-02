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

class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  explicit SetLocalDescriptionObserver(Future<bool> settable_future)
      : settable_future_(std::move(settable_future)) {}

  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    NEARBY_LOG(INFO,
               "SetLocalDescriptionObserver::SetLocalDescriptionObserver()");
    // On success, |error.ok()| is true.
    if (error.ok()) {
      settable_future_.Set(true);
      return;
    }

    NEARBY_LOG(ERROR, "Error when setting local session description: %d %s",
               error.type(), error.message());
    settable_future_.SetException({Exception::kFailed});
  }

 private:
  Future<bool> settable_future_;
};

class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  explicit SetRemoteDescriptionObserver(Future<bool> settable_future)
      : settable_future_(std::move(settable_future)) {}

  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    NEARBY_LOG(
        INFO, "SetRemoteDescriptionObserver::OnSetRemoteDescriptionComplete()");
    // On success, |error.ok()| is true.
    if (error.ok()) {
      NEARBY_LOG(INFO,
                 "SetRemoteDescriptionObserver::OnSetRemoteDescriptionComplete("
                 ") setting future to true");
      settable_future_.Set(true);
      NEARBY_LOG(INFO,
                 "SetRemoteDescriptionObserver::OnSetRemoteDescriptionComplete("
                 ") setting future done");
      return;
    }

    NEARBY_LOG(ERROR, "Error when setting remote session description: %s",
               error.message());
    settable_future_.SetException({Exception::kFailed});
  }

 private:
  Future<bool> settable_future_;
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

  return std::unique_ptr<ConnectionFlow>();
}

ConnectionFlow::ConnectionFlow(
    LocalIceCandidateListener local_ice_candidate_listener,
    DataChannelListener data_channel_listener)
    : data_channel_listener_(std::move(data_channel_listener)),
      peer_connection_observer_(this, std::move(local_ice_candidate_listener)) {
}

ConnectionFlow::~ConnectionFlow() { Close(); }

ConnectionFlow::State ConnectionFlow::GetState() { return state_; }

SessionDescriptionWrapper ConnectionFlow::CreateOffer() {
  auto success_future = Future<SessionDescriptionWrapper>();
  bool will_run = RunOnSignalingThread([this, success_future]() mutable {
    if (!TransitionState(State::kInitialized, State::kCreatingOffer)) {
      success_future.Set(SessionDescriptionWrapper());
      return;
    }

    webrtc::DataChannelInit data_channel_init;
    data_channel_init.reliable = true;
    NEARBY_LOG(INFO,
               "ConnectionFlow::CreateOffer() "
               "peer_connection_->CreateDataChannel(...)");
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel =
        peer_connection_->CreateDataChannel(kDataChannelName,
                                            &data_channel_init);
    RegisterDataChannelObserver(data_channel);

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer =
        new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
            this, success_future, State::kWaitingForAnswer);
    NEARBY_LOG(
        INFO,
        "ConnectionFlow::CreateOffer() peer_connection_->CreateOffer(...)");
    peer_connection_->CreateOffer(observer, options);
  });
  if (!will_run) return SessionDescriptionWrapper();

  ExceptionOr<SessionDescriptionWrapper> result = success_future.Get(kTimeout);
  if (result.ok()) {
    return std::move(result.result());
  }

  NEARBY_LOG(ERROR, "Failed to create offer: %d", result.exception());
  return SessionDescriptionWrapper();
}

SessionDescriptionWrapper ConnectionFlow::CreateAnswer() {
  Future<SessionDescriptionWrapper> success_future;
  bool will_run = RunOnSignalingThread([this, success_future]() mutable {
    if (!TransitionState(State::kReceivedOffer, State::kCreatingAnswer)) {
      success_future.Set(SessionDescriptionWrapper());
      return;
    }

    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer =
        new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
            this, std::move(success_future), State::kWaitingToConnect);
    NEARBY_LOG(
        INFO,
        "ConnectionFlow::CreateAnswer() peer_connection_->CreateAnswer(...)");
    peer_connection_->CreateAnswer(observer, options);
  });
  if (!will_run) return SessionDescriptionWrapper();

  ExceptionOr<SessionDescriptionWrapper> result = success_future.Get(kTimeout);
  if (result.ok()) {
    return std::move(result.result());
  }

  NEARBY_LOG(ERROR, "Failed to create answer: %d", result.exception());
  return SessionDescriptionWrapper();
}

bool ConnectionFlow::SetLocalSessionDescription(
    SessionDescriptionWrapper sdp_wrapper) {
  Future<bool> success_future;
  bool will_run = RunOnSignalingThread(
      [this, success_future, sdp = std::move(sdp_wrapper)]() mutable {
        if (!sdp.IsValid()) {
          success_future.Set(false);
          return;
        }
        rtc::scoped_refptr<SetLocalDescriptionObserver> observer =
            new rtc::RefCountedObject<SetLocalDescriptionObserver>(
                std::move(success_future));

        NEARBY_LOG(INFO,
                   "ConnectionFlow::SetLocalSessionDescription() "
                   "peer_connection_->SetLocalDescription(...)");
        peer_connection_->SetLocalDescription(
            std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
            observer);
      });
  if (!will_run) return false;

  ExceptionOr<bool> result = success_future.Get(kTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set local session description: %d",
               result.exception());
  }
  return success;
}

bool ConnectionFlow::SetRemoteSessionDescription(
    SessionDescriptionWrapper sdp_wrapper, State new_state) {
  Future<bool> success_future;
  bool will_run = RunOnSignalingThread([this, success_future,
                                        sdp = std::move(sdp_wrapper),
                                        new_state]() mutable {
    if (!sdp.IsValid()) {
      success_future.Set(false);
      return;
    }

    TransitionState(state_, new_state);

    rtc::scoped_refptr<SetRemoteDescriptionObserver> observer =
        new rtc::RefCountedObject<SetRemoteDescriptionObserver>(success_future);

    NEARBY_LOG(INFO,
               "ConnectionFlow::SetRemoteSessionDescription() lambda "
               "peer_connection_->SetRemoteDescription(...)");
    peer_connection_->SetRemoteDescription(
        std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
        observer);
    NEARBY_LOG(INFO,
               "ConnectionFlow::SetRemoteSessionDescription() lambda "
               "peer_connection_->SetRemoteDescription(...) done");
  });
  if (!will_run) return false;

  NEARBY_LOG(INFO,
             "ConnectionFlow::SetRemoteSessionDescription() about to wait on "
             "success_future");
  ExceptionOr<bool> result = success_future.Get(kTimeout);
  NEARBY_LOG(INFO,
             "ConnectionFlow::SetRemoteSessionDescription() future is done");
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set remote description: %d",
               result.exception());
  }
  NEARBY_LOG(
      INFO, "ConnectionFlow::SetRemoteSessionDescription() returning success.");
  return success;
}

bool ConnectionFlow::OnOfferReceived(SessionDescriptionWrapper offer) {
  NEARBY_LOG(INFO,
             "ConnectionFlow::OnOfferReceived() setting remote description.");
  return SetRemoteSessionDescription(std::move(offer), State::kReceivedOffer);
}

bool ConnectionFlow::OnAnswerReceived(SessionDescriptionWrapper answer) {
  NEARBY_LOG(INFO,
             "ConnectionFlow::OnOfferReceived() setting remote description.");
  return SetRemoteSessionDescription(std::move(answer),
                                     State::kWaitingToConnect);
}

bool ConnectionFlow::OnRemoteIceCandidatesReceived(
    std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
        source_ice_candidates) {
  // We can't use RunOnSignalingThread here because we need to capture
  // |source_ice_candidates| by move.
  if (peer_connection_->signaling_thread()->IsCurrent()) {
    return false;
  }

  peer_connection_->signaling_thread()->PostTask(
      RTC_FROM_HERE,
      [this, ice_candidates = std::move(source_ice_candidates)]() mutable {
        if (state_ == State::kEnded) {
          NEARBY_LOG(
              WARNING,
              "You cannot add ice candidates to a disconnected session.");
          return;
        }

        if (state_ != State::kWaitingToConnect && state_ != State::kConnected) {
          NEARBY_LOG(INFO,
                     "ConnectionFlow::OnRemoteIceCandidatesReceived() caching "
                     "ice candidates since we are not connected yet.");
          cached_remote_ice_candidates_.insert(
              cached_remote_ice_candidates_.end(),
              std::make_move_iterator(ice_candidates.begin()),
              std::make_move_iterator(ice_candidates.end()));
          return;
        }
        for (auto&& ice_candidate : ice_candidates) {
          NEARBY_LOG(INFO,
                     "ConnectionFlow::OnRemoteIceCandidatesReceived() "
                     "peer_connection_->AddIceCandidate(...)");
          if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
            NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
          }
        }
      });
  return true;
}

bool ConnectionFlow::Close() {
  // BLOCKs calling thread until this completes!
  Future<bool> future;
  bool will_run = RunOnSignalingThread([this, future]() mutable {
    NEARBY_LOG(INFO,
               "ConnectionFlow::Close() lambda Closing WebRTC connection on "
               "signaling thread");
    if (state_ == State::kEnded) {
      NEARBY_LOG(INFO,
                 "ConnectionFlow::Close() lambda in kEnded, early return");
      future.Set(false);
      return;
    }
    state_ = State::kEnded;

    single_threaded_signaling_offloader_.Shutdown();

    NEARBY_LOG(INFO,
               "ConnectionFlow::Close() lambda peer_connection_->Close(...)");
    if (peer_connection_) peer_connection_->Close();

    weak_factory_.InvalidateWeakPtrs();

    data_channel_observer_.reset();
    NEARBY_LOG(INFO,
               "ConnectionFlow::Close() lambda re-setting peer_connection_");
    peer_connection_ = nullptr;
    NEARBY_LOG(INFO, "ConnectionFlow::Close() lambda done");
    future.Set(true);
  });
  if (will_run) {
    NEARBY_LOG(INFO, "ConnectionFlow::Close() BLOCK waiting");
    future.Get();
    NEARBY_LOG(INFO, "ConnectionFlow::Close() UNBLOCK done");
  } else {
    NEARBY_LOG(INFO, "ConnectionFlow::Close() skipped shutdown");
  }
  return true;
}

bool ConnectionFlow::InitPeerConnection(WebRtcMedium& webrtc_medium) {
  NEARBY_LOG(INFO,
             "ConnectionFlow::InitPeerConnection() creating peer_connection_ "
             "webrtc_medium.CreatePeerConnection()");
  Future<bool> success_future;
  // CreatePeerConnection callback may be invoked after ConnectionFlow lifetime
  // has ended, in case of a timeout. Future is captured by value, and is safe
  // to access, but it is not safe to access ConnectionFlow member variables
  // unless the Future::Set() returns true.
  webrtc_medium.CreatePeerConnection(
      &peer_connection_observer_,
      [this, success_future](rtc::scoped_refptr<webrtc::PeerConnectionInterface>
                                 peer_connection) mutable {
        NEARBY_LOG(INFO,
                   "ConnectionFlow::InitPeerConnection() lambda: "
                   "peer_connection_ created");
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

  NEARBY_LOG(INFO,
             "ConnectionFlow::InitPeerConnection() BLOCK! waiting for "
             "peer_connection_ future");
  ExceptionOr<bool> result = success_future.Get(kPeerConnectionTimeout);
  NEARBY_LOG(INFO,
             "ConnectionFlow::InitPeerConnection() future complete, "
             "peer_connection_ should be set.");
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to create peer connection: %d",
               result.exception());
  }
  return success;
}

void ConnectionFlow::OnSignalingStable() {
  CHECK(rtc::Thread::Current() == peer_connection_->signaling_thread());
  NEARBY_LOG(INFO, "ConnectionFlow::OnSignalingStable() called");

  if (state_ != State::kWaitingToConnect && state_ != State::kConnected) return;

  for (auto&& ice_candidate : cached_remote_ice_candidates_) {
    NEARBY_LOG(INFO,
               "ConnectionFlow::OnSignalingStable() "
               "peer_connection_->AddIceCandidate(...)");
    if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
      NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
    }
  }
  cached_remote_ice_candidates_.clear();
}

void ConnectionFlow::ProcessOnPeerConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  CHECK(rtc::Thread::Current() == peer_connection_->signaling_thread());
  NEARBY_LOG(INFO, "ConnectionFlow::ProcessOnPeerConnectionChange() called");
  if (new_state == PeerConnectionState::kFailed ||
      new_state == PeerConnectionState::kDisconnected) {
    Close();
  }
}

void ConnectionFlow::ProcessDataChannelConnected(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  CHECK(rtc::Thread::Current() == peer_connection_->signaling_thread());
  NEARBY_LOG(INFO, "ConnectionFlow::ProcessDataChannelConnected() called");
  NEARBY_LOG(INFO, "Data channel state changed to connected.");
  if (!TransitionState(State::kWaitingToConnect, State::kConnected)) {
    data_channel->Close();
    return;
  }

  // Avoid blocking the signaling thread.
  OffloadFromSignalingThread([this, data_channel{std::move(data_channel)}]() {
    NEARBY_LOG(INFO,
               "ConnectionFlow::ProcessDataChannelConnected() lambda calls "
               "data_channel_listener created cb");
    data_channel_listener_.data_channel_created_cb(std::move(data_channel));
  });
}

void ConnectionFlow::RegisterDataChannelObserver(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (!data_channel_observer_) {
    auto state_change_callback = [this, data_channel]() {
      if (data_channel->state() ==
          webrtc::DataChannelInterface::DataState::kOpen) {
        ProcessDataChannelConnected(std::move(data_channel));
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
  CHECK(rtc::Thread::Current() == peer_connection_->signaling_thread());
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

void ConnectionFlow::OffloadFromSignalingThread(Runnable runnable) {
  single_threaded_signaling_offloader_.Execute(std::move(runnable));
}

bool ConnectionFlow::RunOnSignalingThread(Runnable runnable) {
  if (!peer_connection_ || !peer_connection_->signaling_thread()) {
    NEARBY_LOG(ERROR,
               "Cannot schedule task on signaling thread because the peer "
               "connection is not available");
    return false;
  }
  if (peer_connection_->signaling_thread()->IsCurrent()) {
    // We already on the signaling thread so it is safe to run the task now.
    runnable();
    return true;
  }
  // It is possible for tasks to end up running on the signaling thread after
  // this object
  peer_connection_->signaling_thread()->PostTask(
      RTC_FROM_HERE,
      [weak_ptr = weak_factory_.GetWeakPtr(), task = std::move(runnable)] {
        // don't run the task if the weak_ptr is no longer valid.
        if (!weak_ptr) {
          return;
        }
        task();
      });
  // peer_connection_->signaling_thread()->PostTask(RTC_FROM_HERE,
  // std::move(runnable));
  return true;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
