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

#include "core_v2/internal/mediums/webrtc.h"

#include <functional>
#include <memory>

#include "core_v2/internal/mediums/webrtc/session_description_wrapper.h"
#include "core_v2/internal/mediums/webrtc/signaling_frames.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/listeners.h"
#include "platform_v2/public/cancelable_alarm.h"
#include "platform_v2/public/future.h"
#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex_lock.h"
#include "location/nearby/mediums/proto/web_rtc_signaling_frames.pb.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "webrtc/api/jsep.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

// The maximum amount of time to wait to connect to a data channel via WebRTC.
constexpr absl::Duration kDataChannelTimeout = absl::Milliseconds(5000);

// Delay between restarting signaling messenger to receive messages.
constexpr absl::Duration kRestartReceiveMessagesDuration = absl::Seconds(60);

}  // namespace

WebRtc::WebRtc() = default;

WebRtc::~WebRtc() {
  // This ensures that all pending callbacks are run before we reset the medium
  // and we are not accepting new runnables.
  restart_receive_messages_executor_.Shutdown();
  single_thread_executor_.Shutdown();

  Disconnect();
}

bool WebRtc::IsAvailable() { return medium_.IsValid(); }

bool WebRtc::IsAcceptingConnections() {
  MutexLock lock(&mutex_);
  return role_ == Role::kOfferer;
}

bool WebRtc::StartAcceptingConnections(const PeerId& self_id,
                                       AcceptedConnectionCallback callback) {
  if (!IsAvailable()) {
    {
      MutexLock lock(&mutex_);
      LogAndDisconnect("WebRTC is not available for data transfer.");
    }
    return false;
  }

  if (IsAcceptingConnections()) {
    NEARBY_LOG(WARNING, "Already accepting WebRTC connections.");
    return false;
  }

  {
    MutexLock lock(&mutex_);
    if (role_ != Role::kNone) {
      NEARBY_LOG(WARNING,
                 "Cannot start accepting WebRTC connections, current role %d",
                 role_);
      return false;
    }

    if (!InitWebRtcFlow(Role::kOfferer, self_id)) return false;

    restart_receive_messages_alarm_ = CancelableAlarm(
        "restart_receiving_messages_webrtc",
        std::bind(&WebRtc::RestartReceiveMessages, this),
        kRestartReceiveMessagesDuration, &restart_receive_messages_executor_);

    SessionDescriptionWrapper offer = connection_flow_->CreateOffer();
    pending_local_offer_ = webrtc_frames::EncodeOffer(self_id, offer.GetSdp());
    if (!SetLocalSessionDescription(std::move(offer))) {
      return false;
    }

    // There is no timeout set for the future returned since we do not know how
    // much time it will take for the two devices to discover each other before
    // the actual transport can begin.
    ListenForWebRtcSocketFuture(connection_flow_->GetDataChannel(),
                                std::move(callback));
    NEARBY_LOG(INFO, "Started listening for WebRtc connections as %s",
               self_id.GetId().c_str());
  }

  return true;
}

WebRtcSocketWrapper WebRtc::Connect(const PeerId& peer_id) {
  if (!IsAvailable()) {
    Disconnect();
    return WebRtcSocketWrapper();
  }

  {
    MutexLock lock(&mutex_);
    if (role_ != Role::kNone) {
      NEARBY_LOG(
          WARNING,
          "Cannot connect with WebRtc because we are already acting as %d",
          role_);
      return WebRtcSocketWrapper();
    }

    peer_id_ = peer_id;
    if (!InitWebRtcFlow(Role::kAnswerer, PeerId::FromRandom())) {
      return WebRtcSocketWrapper();
    }
  }

  NEARBY_LOG(INFO, "Attempting to make a WebRTC connection to %s.",
             peer_id.GetId().c_str());

  Future<WebRtcSocketWrapper> socket_future = ListenForWebRtcSocketFuture(
      connection_flow_->GetDataChannel(), AcceptedConnectionCallback());

  // The two devices have discovered each other, hence we have a timeout for
  // establishing the transport channel.
  // NOTE - We should not hold |mutex_| while waiting for the data channel since
  // it would block incoming signaling messages from being processed, resulting
  // in a timeout in creating the socket.
  ExceptionOr<WebRtcSocketWrapper> result =
      socket_future.Get(kDataChannelTimeout);
  if (result.ok()) return result.result();

  Disconnect();
  return WebRtcSocketWrapper();
}

bool WebRtc::SetLocalSessionDescription(SessionDescriptionWrapper sdp) {
  if (!connection_flow_->SetLocalSessionDescription(std::move(sdp))) {
    LogAndDisconnect("Unable to set local session description");
    return false;
  }

  return true;
}

void WebRtc::StopAcceptingConnections() {
  if (!IsAcceptingConnections()) {
    NEARBY_LOG(INFO,
               "Skipped StopAcceptingConnections since we are not currently "
               "accepting WebRTC connections");
    return;
  }

  {
    MutexLock lock(&mutex_);
    ShutdownSignaling();
  }
  NEARBY_LOG(INFO, "Stopped accepting WebRTC connections");
}

Future<WebRtcSocketWrapper> WebRtc::ListenForWebRtcSocketFuture(
    Future<rtc::scoped_refptr<webrtc::DataChannelInterface>>
        data_channel_future,
    AcceptedConnectionCallback callback) {
  Future<WebRtcSocketWrapper> socket_future;
  auto data_channel_runnable = [this, socket_future, data_channel_future,
                                callback{std::move(callback)}]() mutable {
    // The overall timeout of creating the socket and data channel is controlled
    // by the caller of this function.
    ExceptionOr<rtc::scoped_refptr<webrtc::DataChannelInterface>> res =
        data_channel_future.Get();
    if (res.ok()) {
      WebRtcSocketWrapper wrapper = CreateWebRtcSocketWrapper(res.result());
      callback.accepted_cb(wrapper);
      {
        MutexLock lock(&mutex_);
        socket_ = wrapper;
      }
      socket_future.Set(wrapper);
    } else {
      NEARBY_LOG(WARNING, "Failed to get WebRtcSocket.");
      socket_future.Set(WebRtcSocketWrapper());
    }
  };

  data_channel_future.AddListener(std::move(data_channel_runnable),
                                  &single_thread_executor_);

  return socket_future;
}

WebRtcSocketWrapper WebRtc::CreateWebRtcSocketWrapper(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (data_channel == nullptr) {
    return WebRtcSocketWrapper();
  }

  auto socket = std::make_unique<WebRtcSocket>("WebRtcSocket", data_channel);
  socket->SetOnSocketClosedListener(
      {[this]() { OffloadFromSignalingThread([this]() { Disconnect(); }); }});
  return WebRtcSocketWrapper(std::move(socket));
}

bool WebRtc::InitWebRtcFlow(Role role, const PeerId& self_id) {
  role_ = role;
  self_id_ = self_id;

  if (connection_flow_) {
    LogAndShutdownSignaling(
        "Tried to initialize WebRTC without shutting down the previous "
        "connection");
    return false;
  }

  if (signaling_messenger_) {
    LogAndShutdownSignaling(
        "Tried to initialize WebRTC without shutting down signaling messenger");
    return false;
  }

  signaling_messenger_ = medium_.GetSignalingMessenger(self_id_.GetId());
  auto signaling_message_callback = [this](ByteArray message) {
    OffloadFromSignalingThread([this, message{std::move(message)}]() {
      ProcessSignalingMessage(message);
    });
  };

  if (!signaling_messenger_->IsValid() ||
      !signaling_messenger_->StartReceivingMessages(
          signaling_message_callback)) {
    DisconnectLocked();
    return false;
  }

  if (role_ == Role::kAnswerer &&
      !signaling_messenger_->SendMessage(
          peer_id_.GetId(),
          webrtc_frames::EncodeReadyForSignalingPoke(self_id))) {
    LogAndDisconnect(absl::StrCat("Could not send signaling poke to peer ",
                                  peer_id_.GetId()));
    return false;
  }

  connection_flow_ = ConnectionFlow::Create(GetLocalIceCandidateListener(),
                                            GetDataChannelListener(), medium_);
  if (!connection_flow_)
      return false;

  return true;
}

void WebRtc::OnLocalIceCandidate(
    const webrtc::IceCandidateInterface* local_ice_candidate) {
  ::location::nearby::mediums::IceCandidate ice_candidate =
      webrtc_frames::EncodeIceCandidate(*local_ice_candidate);

  OffloadFromSignalingThread([this, ice_candidate{std::move(ice_candidate)}]() {
    MutexLock lock(&mutex_);
    if (IsSignaling()) {
      signaling_messenger_->SendMessage(
          peer_id_.GetId(), webrtc_frames::EncodeIceCandidates(
                                self_id_, {std::move(ice_candidate)}));
    } else {
      pending_local_ice_candidates_.push_back(std::move(ice_candidate));
    }
  });
}

LocalIceCandidateListener WebRtc::GetLocalIceCandidateListener() {
  return {std::bind(&WebRtc::OnLocalIceCandidate, this, std::placeholders::_1)};
}

void WebRtc::OnDataChannelClosed() {
  OffloadFromSignalingThread([this]() {
    MutexLock lock(&mutex_);
    LogAndDisconnect("WebRTC data channel closed");
  });
}

void WebRtc::OnDataChannelMessageReceived(const ByteArray& message) {
  OffloadFromSignalingThread([this, message]() {
    MutexLock lock(&mutex_);
    if (!socket_.IsValid()) {
      LogAndDisconnect("Received a data channel message without a socket");
      return;
    }

    socket_.NotifyDataChannelMsgReceived(message);
  });
}

void WebRtc::OnDataChannelBufferedAmountChanged() {
  OffloadFromSignalingThread([this]() {
    MutexLock lock(&mutex_);
    if (!socket_.IsValid()) {
      LogAndDisconnect("Data channel buffer changed without a socket");
      return;
    }

    socket_.NotifyDataChannelBufferedAmountChanged();
  });
}

DataChannelListener WebRtc::GetDataChannelListener() {
  return {
      .data_channel_closed_cb = std::bind(&WebRtc::OnDataChannelClosed, this),
      .data_channel_message_received_cb = std::bind(
          &WebRtc::OnDataChannelMessageReceived, this, std::placeholders::_1),
      .data_channel_buffered_amount_changed_cb =
          std::bind(&WebRtc::OnDataChannelBufferedAmountChanged, this),
  };
}

bool WebRtc::IsSignaling() {
  return (role_ != Role::kNone && self_id_.IsValid() && peer_id_.IsValid());
}

void WebRtc::ProcessSignalingMessage(const ByteArray& message) {
  MutexLock lock(&mutex_);

  if (!connection_flow_) {
    LogAndDisconnect("Received WebRTC frame before signaling was started");
    return;
  }

  location::nearby::mediums::WebRtcSignalingFrame frame;
  if (!frame.ParseFromString(std::string(message))) {
    LogAndDisconnect("Failed to parse signaling message");
    return;
  }

  if (!frame.has_sender_id()) {
    LogAndDisconnect("Invalid WebRTC frame: Sender ID is missing");
    return;
  }

  if (frame.has_ready_for_signaling_poke() && !peer_id_.IsValid()) {
    peer_id_ = PeerId(frame.sender_id().id());
    NEARBY_LOG(INFO, "Peer %s is ready for signaling",
               peer_id_.GetId().c_str());
  }

  if (!IsSignaling()) {
    NEARBY_LOG(INFO,
               "Ignoring WebRTC frame: we are not currently listening for "
               "signaling messages");
    return;
  }

  if (frame.sender_id().id() != peer_id_.GetId()) {
    NEARBY_LOG(
        INFO, "Ignoring WebRTC frame: we are only listening for another peer.");
    return;
  }

  if (frame.has_ready_for_signaling_poke()) {
    SendOfferAndIceCandidatesToPeer();
  } else if (frame.has_offer()) {
    connection_flow_->OnOfferReceived(
        SessionDescriptionWrapper(webrtc_frames::DecodeOffer(frame).release()));
    SendAnswerToPeer();
  } else if (frame.has_answer()) {
    connection_flow_->OnAnswerReceived(SessionDescriptionWrapper(
        webrtc_frames::DecodeAnswer(frame).release()));
  } else if (frame.has_ice_candidates()) {
    if (!connection_flow_->OnRemoteIceCandidatesReceived(
            webrtc_frames::DecodeIceCandidates(frame))) {
      LogAndDisconnect("Could not add remote ice candidates.");
    }
  }
}

void WebRtc::SendOfferAndIceCandidatesToPeer() {
  if (pending_local_offer_.Empty()) {
    LogAndDisconnect(
        "Unable to send pending offer to remote peer: local offer not set");
    return;
  }

  if (!signaling_messenger_->SendMessage(peer_id_.GetId(),
                                         pending_local_offer_)) {
    LogAndDisconnect("Failed to send local offer via signaling messenger");
    return;
  }
  pending_local_offer_ = ByteArray();

  if (!pending_local_ice_candidates_.empty()) {
    signaling_messenger_->SendMessage(
        peer_id_.GetId(),
        webrtc_frames::EncodeIceCandidates(
            self_id_, std::move(pending_local_ice_candidates_)));
  }
}

void WebRtc::SendAnswerToPeer() {
  SessionDescriptionWrapper answer = connection_flow_->CreateAnswer();
  ByteArray answer_message(
      webrtc_frames::EncodeAnswer(self_id_, answer.GetSdp()));

  if (!SetLocalSessionDescription(std::move(answer))) return;

  if (!signaling_messenger_->SendMessage(peer_id_.GetId(), answer_message)) {
    LogAndDisconnect("Failed to send local answer via signaling messenger");
    return;
  }
}

void WebRtc::LogAndDisconnect(const std::string& error_message) {
  NEARBY_LOG(WARNING, "Disconnecting WebRTC : %s", error_message.c_str());
  DisconnectLocked();
}

void WebRtc::LogAndShutdownSignaling(const std::string& error_message) {
  NEARBY_LOG(WARNING, "Stopping WebRTC signaling : %s", error_message.c_str());
  ShutdownSignaling();
}

void WebRtc::ShutdownSignaling() {
  role_ = Role::kNone;
  self_id_ = PeerId();
  peer_id_ = PeerId();
  pending_local_offer_ = ByteArray();
  pending_local_ice_candidates_.clear();

  if (restart_receive_messages_alarm_.IsValid()) {
    restart_receive_messages_alarm_.Cancel();
    restart_receive_messages_alarm_ = CancelableAlarm();
  }

  if (signaling_messenger_) {
    signaling_messenger_->StopReceivingMessages();
    signaling_messenger_.reset();
  }

  if (!socket_.IsValid()) ShutdownIceCandidateCollection();
}

void WebRtc::Disconnect() {
  MutexLock lock(&mutex_);
  DisconnectLocked();
}

void WebRtc::DisconnectLocked() {
  ShutdownSignaling();
  ShutdownWebRtcSocket();
  ShutdownIceCandidateCollection();
}

void WebRtc::ShutdownWebRtcSocket() {
  if (socket_.IsValid()) {
    socket_.Close();
    socket_ = WebRtcSocketWrapper();
  }
}

void WebRtc::ShutdownIceCandidateCollection() {
  if (connection_flow_) {
    connection_flow_->Close();
    connection_flow_.reset();
  }
}

void WebRtc::OffloadFromSignalingThread(Runnable runnable) {
  single_thread_executor_.Execute(std::move(runnable));
}

void WebRtc::RestartReceiveMessages() {
  if (!IsAcceptingConnections()) {
    NEARBY_LOG(INFO,
               "Skipping restart since we are not accepting connections.");
    return;
  }

  NEARBY_LOG(INFO, "Restarting listening for receiving signaling messages.");
  {
    MutexLock lock(&mutex_);
    signaling_messenger_->StopReceivingMessages();

    signaling_messenger_ = medium_.GetSignalingMessenger(self_id_.GetId());

    auto signaling_message_callback = [this](ByteArray message) {
      OffloadFromSignalingThread([this, message{std::move(message)}]() {
        ProcessSignalingMessage(message);
      });
    };

    if (!signaling_messenger_->IsValid() ||
        !signaling_messenger_->StartReceivingMessages(
            signaling_message_callback)) {
      DisconnectLocked();
    }
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
