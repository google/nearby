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

#ifndef NO_WEBRTC

#include "connections/implementation/mediums/webrtc/connection_flow.h"

#include <iterator>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/webrtc/session_description_wrapper.h"
#include "connections/implementation/mediums/webrtc/webrtc_socket_impl.h"
#include "connections/implementation/mediums/webrtc_socket.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/webrtc.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"

namespace nearby {
namespace connections {
namespace mediums {

constexpr absl::Duration ConnectionFlow::kTimeout;
constexpr absl::Duration ConnectionFlow::kPeerConnectionTimeout;

// This is the same as the nearby data channel name.
constexpr char kDataChannelName[] = "dataChannel";

class CreateSessionDescriptionObserverImpl
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserverImpl(
      ConnectionFlow* connection_flow,
      Future<SessionDescriptionWrapper> settable_future,
      ConnectionFlow::State expected_entry_state,
      ConnectionFlow::State exit_state)
      : connection_flow_{connection_flow},
        settable_future_{settable_future},
        expected_entry_state_{expected_entry_state},
        exit_state_{exit_state} {}

  // webrtc::CreateSessionDescriptionObserver
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (connection_flow_->TransitionState(expected_entry_state_, exit_state_)) {
      settable_future_.Set(SessionDescriptionWrapper{desc});
    } else {
      settable_future_.SetException({Exception::kFailed});
    }
  }

  void OnFailure(webrtc::RTCError error) override {
    NEARBY_LOG(ERROR, "Error when creating session description: %s",
               error.message());
    settable_future_.SetException({Exception::kFailed});
  }

 private:
  ConnectionFlow* connection_flow_;
  Future<SessionDescriptionWrapper> settable_future_;
  ConnectionFlow::State expected_entry_state_;
  ConnectionFlow::State exit_state_;
};

class SetDescriptionObserverBase {
 public:
  ExceptionOr<bool> GetResult(absl::Duration timeout) {
    return settable_future_.Get(timeout);
  }

 protected:
  void OnSetDescriptionComplete(webrtc::RTCError error) {
    // On success, |error.ok()| is true.
    if (error.ok()) {
      settable_future_.Set(true);
      return;
    }
    settable_future_.SetException({Exception::kFailed});
  }

 private:
  Future<bool> settable_future_;
};

class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface,
      public SetDescriptionObserverBase {
 public:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    OnSetDescriptionComplete(error);
  }
};

class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface,
      public SetDescriptionObserverBase {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    OnSetDescriptionComplete(error);
  }
};

using PeerConnectionState =
    webrtc::PeerConnectionInterface::PeerConnectionState;

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

ConnectionFlow::~ConnectionFlow() {
  NEARBY_LOG(INFO, "~ConnectionFlow");
  RunOnSignalingThread([this] { CloseOnSignalingThread(); });
  shutdown_latch_.Await();
  NEARBY_LOG(INFO, "~ConnectionFlow done");
}

SessionDescriptionWrapper ConnectionFlow::CreateOffer() {
  CHECK(!IsRunningOnSignalingThread());
  Future<SessionDescriptionWrapper> success_future;
  if (!RunOnSignalingThread([this, success_future] {
        CreateOfferOnSignalingThread(success_future);
      })) {
    NEARBY_LOG(ERROR, "Failed to create offer");
    return SessionDescriptionWrapper();
  }
  ExceptionOr<SessionDescriptionWrapper> result = success_future.Get(kTimeout);
  if (result.ok()) {
    return std::move(result.result());
  }
  NEARBY_LOG(ERROR, "Failed to create offer: %d", result.exception());
  return SessionDescriptionWrapper();
}

void ConnectionFlow::CreateOfferOnSignalingThread(
    Future<SessionDescriptionWrapper> success_future) {
  if (!TransitionState(State::kInitialized, State::kCreatingOffer)) {
    success_future.SetException({Exception::kFailed});
    return;
  }
  webrtc::DataChannelInit data_channel_init;
  data_channel_init.reliable = true;
  auto pc = GetPeerConnection();
  auto result =
      pc->CreateDataChannelOrError(kDataChannelName, &data_channel_init);
  if (!result.ok()) {
    success_future.SetException({Exception::kFailed});
    return;
  }
  CreateSocketFromDataChannel(result.MoveValue());

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer(
      new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
          this, success_future, State::kCreatingOffer,
          State::kWaitingForAnswer));
  pc->CreateOffer(observer.get(), options);
}

SessionDescriptionWrapper ConnectionFlow::CreateAnswer() {
  CHECK(!IsRunningOnSignalingThread());
  Future<SessionDescriptionWrapper> success_future;
  if (!RunOnSignalingThread([this, success_future] {
        CreateAnswerOnSignalingThread(success_future);
      })) {
    NEARBY_LOG(ERROR, "Failed to create answer");
    return SessionDescriptionWrapper();
  }
  ExceptionOr<SessionDescriptionWrapper> result = success_future.Get(kTimeout);
  if (result.ok()) {
    return std::move(result.result());
  }
  NEARBY_LOG(ERROR, "Failed to create answer: %d", result.exception());
  return SessionDescriptionWrapper();
}

void ConnectionFlow::CreateAnswerOnSignalingThread(
    Future<SessionDescriptionWrapper> success_future) {
  if (!TransitionState(State::kReceivedOffer, State::kCreatingAnswer)) {
    success_future.SetException({Exception::kFailed});
    return;
  }
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  rtc::scoped_refptr<CreateSessionDescriptionObserverImpl> observer(
      new rtc::RefCountedObject<CreateSessionDescriptionObserverImpl>(
          this, success_future, State::kCreatingAnswer,
          State::kWaitingToConnect));
  auto pc = GetPeerConnection();
  pc->CreateAnswer(observer.get(), options);
}

bool ConnectionFlow::SetLocalSessionDescription(SessionDescriptionWrapper sdp) {
  CHECK(!IsRunningOnSignalingThread());
  if (!sdp.IsValid()) return false;

  rtc::scoped_refptr<SetLocalDescriptionObserver> observer(
      new rtc::RefCountedObject<SetLocalDescriptionObserver>());

  if (!RunOnSignalingThread([this, observer, sdp = std::move(sdp)]() mutable {
        if (state_ == State::kEnded) {
          observer->OnSetLocalDescriptionComplete(
              webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE));
          return;
        }
        auto pc = GetPeerConnection();

        pc->SetLocalDescription(
            std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
            observer);
      })) {
    return false;
  }

  ExceptionOr<bool> result = observer->GetResult(kTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set local session description: %d",
               result.exception());
  }
  return success;
}

bool ConnectionFlow::SetRemoteSessionDescription(SessionDescriptionWrapper sdp,
                                                 State expected_entry_state,
                                                 State exit_state) {
  if (!sdp.IsValid()) return false;

  rtc::scoped_refptr<SetRemoteDescriptionObserver> observer(
      new rtc::RefCountedObject<SetRemoteDescriptionObserver>());

  if (!RunOnSignalingThread([this, observer, sdp = std::move(sdp),
                             expected_entry_state, exit_state]() mutable {
        if (!TransitionState(expected_entry_state, exit_state)) {
          observer->OnSetRemoteDescriptionComplete(
              webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE));
          return;
        }
        auto pc = GetPeerConnection();

        pc->SetRemoteDescription(
            std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp.Release()),
            observer);
      })) {
    return false;
  }

  ExceptionOr<bool> result = observer->GetResult(kTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    NEARBY_LOG(ERROR, "Failed to set remote description: %d",
               result.exception());
  }
  return success;
}

bool ConnectionFlow::OnOfferReceived(SessionDescriptionWrapper offer) {
  CHECK(!IsRunningOnSignalingThread());
  return SetRemoteSessionDescription(std::move(offer), State::kInitialized,
                                     State::kReceivedOffer);
}

bool ConnectionFlow::OnAnswerReceived(SessionDescriptionWrapper answer) {
  CHECK(!IsRunningOnSignalingThread());
  return SetRemoteSessionDescription(
      std::move(answer), State::kWaitingForAnswer, State::kWaitingToConnect);
}

bool ConnectionFlow::OnRemoteIceCandidatesReceived(
    std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
        ice_candidates) {
  CHECK(!IsRunningOnSignalingThread());
  // We can't call RunOnSignalingThread because C++ wants to copy ice_candidates
  // if we try. unique_ptr is not CopyConstructible and compilation fails.
  auto pc = GetPeerConnection();

  if (!pc) {
    return false;
  }
  pc->signaling_thread()->PostTask(
      [this, can_run_tasks = std::weak_ptr<void>(can_run_tasks_),
       candidates = std::move(ice_candidates)]() mutable {
        // don't run the task if the weak_ptr is no longer valid.
        if (!can_run_tasks.lock()) {
          return;
        }
        AddIceCandidatesOnSignalingThread(std::move(candidates));
      });
  return true;
}

void ConnectionFlow::AddIceCandidatesOnSignalingThread(
    std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
        ice_candidates) {
  CHECK(IsRunningOnSignalingThread());
  if (state_ == State::kEnded) {
    NEARBY_LOG(WARNING,
               "You cannot add ice candidates to a disconnected session.");
    return;
  }
  if (state_ != State::kWaitingToConnect && state_ != State::kConnected) {
    cached_remote_ice_candidates_.insert(
        cached_remote_ice_candidates_.end(),
        std::make_move_iterator(ice_candidates.begin()),
        std::make_move_iterator(ice_candidates.end()));
    return;
  }
  auto pc = GetPeerConnection();
  for (auto&& ice_candidate : ice_candidates) {
    if (!pc->AddIceCandidate(ice_candidate.get())) {
      NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
    }
  }
}

bool ConnectionFlow::CloseIfNotConnected() {
  CHECK(!IsRunningOnSignalingThread());
  Future<bool> closed;
  if (RunOnSignalingThread([this, closed]() mutable {
        if (state_ == State::kConnected) {
          closed.Set(false);
        } else {
          CloseOnSignalingThread();
          closed.Set(true);
        }
      })) {
    auto result = closed.Get();
    return result.ok() && result.result();
  }
  return true;
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
        MutexLock lock(&mutex_);
        peer_connection_ = peer_connection;
        signaling_thread_for_dcheck_only_ =
            peer_connection_->signaling_thread();
        success_future.Set(true);
      });

  ExceptionOr<bool> result = success_future.Get(kPeerConnectionTimeout);
  bool success = result.ok() && result.result();
  if (!success) {
    shutdown_latch_.CountDown();
    NEARBY_LOG(ERROR, "Failed to create peer connection: %d",
               result.exception());
  }
  return success;
}

void ConnectionFlow::OnSignalingStable() {
  if (state_ != State::kWaitingToConnect && state_ != State::kConnected) return;
  auto pc = GetPeerConnection();
  for (auto&& ice_candidate : cached_remote_ice_candidates_) {
    if (!pc->AddIceCandidate(ice_candidate.get())) {
      NEARBY_LOG(WARNING, "Unable to add remote ice candidate.");
    }
  }
  cached_remote_ice_candidates_.clear();
}

void ConnectionFlow::CreateSocketFromDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  NEARBY_LOG(INFO, "Creating data channel socket");
  auto socket =
      std::make_unique<WebRtcSocket>("WebRtcSocket", std::move(data_channel));
  socket->SetSocketListener({
      .socket_ready_cb = {[this](WebRtcSocket* socket) {
        CHECK(IsRunningOnSignalingThread());
        if (!TransitionState(State::kWaitingToConnect, State::kConnected)) {
          NEARBY_LOG(ERROR,
                     "Data channel socket is open but connection flow was not "
                     "in the required state");
          socket->Close();
          return;
        }
        // Pass socket wrapper by copy on purpose
        data_channel_listener_.data_channel_open_cb(socket_wrapper_);
      }},
      .socket_closed_cb =
          [this](WebRtcSocket*) {
            data_channel_listener_.data_channel_closed_cb();
          },
  });
  socket_wrapper_ = WebRtcSocketWrapper(std::move(socket));
}

void ConnectionFlow::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  CHECK(IsRunningOnSignalingThread());
  local_ice_candidate_listener_.local_ice_candidate_found_cb(candidate);
}

void ConnectionFlow::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  NEARBY_LOG(INFO, "OnSignalingChange: %d", new_state);
  CHECK(IsRunningOnSignalingThread());
  if (new_state == webrtc::PeerConnectionInterface::SignalingState::kStable) {
    OnSignalingStable();
  }
}

void ConnectionFlow::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  NEARBY_LOG(INFO, "OnDataChannel");
  CHECK(IsRunningOnSignalingThread());
  CreateSocketFromDataChannel(std::move(data_channel));
}

void ConnectionFlow::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  NEARBY_LOG(INFO, "OnIceGatheringChange: %d", new_state);
  CHECK(IsRunningOnSignalingThread());
}

void ConnectionFlow::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  NEARBY_LOG(INFO, "OnConnectionChange: %d", new_state);
  CHECK(IsRunningOnSignalingThread());
  if (new_state == PeerConnectionState::kClosed ||
      new_state == PeerConnectionState::kFailed ||
      new_state == PeerConnectionState::kDisconnected) {
    NEARBY_LOG(INFO, "Closing due to peer connection state change: %d",
               new_state);
    CloseOnSignalingThread();
  }
}

void ConnectionFlow::OnRenegotiationNeeded() {
  NEARBY_LOG(INFO, "OnRenegotiationNeeded");
  CHECK(IsRunningOnSignalingThread());
}

bool ConnectionFlow::TransitionState(State current_state, State new_state) {
  CHECK(IsRunningOnSignalingThread());
  if (current_state != state_) {
    NEARBY_LOG(
        WARNING,
        "Invalid state transition to %d: current state is %d but expected %d.",
        new_state, state_, current_state);
    return false;
  }
  NEARBY_LOG(INFO, "Transition: %d -> %d", state_, new_state);
  state_ = new_state;
  return true;
}

bool ConnectionFlow::CloseOnSignalingThread() {
  if (state_ == State::kEnded) {
    return false;
  }
  state_ = State::kEnded;
  // Close the socket wrapper before terminating the PeerConnection
  // since the teardown process of the PC may close threads that are
  // otherwise depended upon by objects kept alive by the socket_wrapper.
  if (socket_wrapper_.IsValid()) socket_wrapper_.Close();

  // This prevents other tasks from queuing on the signaling thread for this
  // object.
  auto pc = GetAndResetPeerConnection();

  NEARBY_LOG(INFO, "Closing WebRTC peer connection.");
  // NOTE: Closing the peer connection will close the data channel and thus the
  // socket implicitly.
  if (pc) pc->Close();
  NEARBY_LOG(INFO, "Closed WebRTC peer connection.");
  // Prevent any already queued tasks from running on the signaling thread
  can_run_tasks_.reset();
  // If anyone was waiting for shutdown to be done let them know.
  shutdown_latch_.CountDown();
  return true;
}

bool ConnectionFlow::RunOnSignalingThread(Runnable&& runnable) {
  CHECK(!IsRunningOnSignalingThread());
  auto pc = GetPeerConnection();
  if (!pc) {
    NEARBY_LOG(WARNING,
               "Peer connection not available. Cannot schedule tasks.");
    return false;
  }
  // We are off signaling thread, so we can't use peer connection's methods
  // but we can access the signaling thread handle.
  pc->signaling_thread()->PostTask(
      [can_run_tasks = std::weak_ptr<void>(can_run_tasks_),
       task = std::move(runnable)]() mutable {
        // don't run the task if the weak_ptr is no longer valid.
        // shared_ptr |can_run_tasks_| is destroyed on the same thread
        // (signaling thread). This guarantees that if the weak_ptr is valid
        // when this task starts, it will stay valid until the task ends.
        if (!can_run_tasks.lock()) {
          NEARBY_LOG(INFO, "Peer connection already closed. Cannot run tasks.");
          return;
        }
        task();
      });
  return true;
}

bool ConnectionFlow::IsRunningOnSignalingThread() {
  return signaling_thread_for_dcheck_only_ != nullptr &&
         signaling_thread_for_dcheck_only_ == rtc::Thread::Current();
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface>
ConnectionFlow::GetPeerConnection() {
  // We must use a mutex to ensure that peer connection is
  // fully initialized.
  // We increase the peer_connection_'s refcount to keep it
  // alive while we use it.
  MutexLock lock(&mutex_);
  return peer_connection_;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface>
ConnectionFlow::GetAndResetPeerConnection() {
  MutexLock lock(&mutex_);
  return std::move(peer_connection_);
}
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif
