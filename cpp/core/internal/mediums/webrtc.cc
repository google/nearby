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

#include "core/internal/mediums/webrtc.h"

#include <functional>
#include <memory>
#include <sstream>

#include "core/internal/mediums/webrtc/session_description_wrapper.h"
#include "core/internal/mediums/webrtc/signaling_frames.h"
#include "platform/base/byte_array.h"
#include "platform/base/listeners.h"
#include "platform/public/cancelable_alarm.h"
#include "platform/public/future.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"
#include "location/nearby/mediums/proto/web_rtc_signaling_frames.pb.h"
#include "absl/container/flat_hash_map.h"
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
  {
    MutexLock lock(&mutex_);
    NEARBY_LOGS(WARNING) << "Destructing Webrtc: " << InternalStatesToString();
  }
  // This ensures that all pending callbacks are run before we reset the medium
  // and we are not accepting new runnables.
  restart_receive_messages_executor_.Shutdown();
  single_thread_executor_.Shutdown();

  // Disconnect will also erase the connection info from map. Use a separate
  // set to save the connection ids to avoid the iterator violation issue.
  absl::flat_hash_set<std::string> connection_ids;
  for (auto& item : accepting_map_) {
    connection_ids.emplace(item.first);
  }
  for (const auto& connection_id : connection_ids) {
    Disconnect(Role::kOfferer, connection_id);
  }
  connection_ids.clear();
  for (auto& item : connecting_map_) {
    connection_ids.emplace(item.first);
  }
  for (const auto& connection_id : connection_ids) {
    Disconnect(Role::kAnswerer, connection_id);
  }
  connection_ids.clear();
}

const std::string WebRtc::GetDefaultCountryCode() {
  return medium_.GetDefaultCountryCode();
}

bool WebRtc::IsAvailable() { return medium_.IsValid(); }

bool WebRtc::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  ConnectionInfo* connection_info =
      GetConnectionInfo(Role::kOfferer, service_id);
  return connection_info && connection_info->self_id.IsValid();
}

bool WebRtc::StartAcceptingConnections(const std::string& service_id,
                                       const PeerId& self_id,
                                       const LocationHint& location_hint,
                                       AcceptedConnectionCallback callback) {
  if (!IsAvailable()) {
    MutexLock lock(&mutex_);
    LogAndDisconnect(Role::kOfferer, service_id,
                     "WebRTC is not available for data transfer.");
    return false;
  }

  if (IsAcceptingConnections(service_id)) {
    NEARBY_LOG(WARNING, "Already accepting WebRTC connections.");
    return false;
  }
  {
    MutexLock lock(&mutex_);
    NEARBY_LOGS(WARNING) << "StartAcceptingConnections: "
                         << InternalStatesToString();
    accepting_map_.emplace(service_id,
                           ConnectionInfo{.socket = WebRtcSocketWrapper()});
    ConnectionInfo* connection_info = &accepting_map_[service_id];
    if (!InitWebRtcFlow(Role::kOfferer, self_id, location_hint, service_id))
      return false;

    connection_info->restart_receive_messages_alarm = CancelableAlarm(
        "restart_receiving_messages_webrtc",
        std::bind(&WebRtc::RestartReceiveMessages, this, location_hint,
                  service_id),
        kRestartReceiveMessagesDuration, &restart_receive_messages_executor_);

    SessionDescriptionWrapper offer =
        connection_info->connection_flow->CreateOffer();
    connection_info->pending_local_offer =
        webrtc_frames::EncodeOffer(self_id, offer.GetSdp());
    if (!SetLocalSessionDescription(std::move(offer), Role::kOfferer,
                                    service_id)) {
      NEARBY_LOG(WARNING, "Failed to set local session description.");
      return false;
    }

    // There is no timeout set for the future returned since we do not know how
    // much time it will take for the two devices to discover each other before
    // the actual transport can begin.
    ListenForWebRtcSocketFuture(
        Role::kOfferer, service_id,
        connection_info->connection_flow->GetDataChannel(),
        std::move(callback));
    NEARBY_LOG(WARNING, "Started listening for WebRtc connections as %s",
               self_id.GetId().c_str());
  }

  return true;
}

WebRtcSocketWrapper WebRtc::Connect(const PeerId& peer_id,
                                    const LocationHint& location_hint) {
  if (!IsAvailable()) {
    MutexLock lock(&mutex_);
    LogAndDisconnect(Role::kAnswerer, peer_id.GetId(),
                     "WebRTC is not available for data transfer.");
    return WebRtcSocketWrapper();
  }

  {
    MutexLock lock(&mutex_);
    NEARBY_LOGS(WARNING) << "Start Connecting to " << peer_id.GetId() << ":\n"
                         << InternalStatesToString();
    if (connecting_map_.contains(peer_id.GetId())) {
      NEARBY_LOG(
          ERROR,
          "Cannot connect with WebRtc because we are already connecting.");
      return WebRtcSocketWrapper();
    }
    connecting_map_.emplace(peer_id.GetId(),
                            ConnectionInfo{.socket = WebRtcSocketWrapper()});
    ConnectionInfo* connection_info = &connecting_map_[peer_id.GetId()];
    connection_info->peer_id = peer_id;
    if (!InitWebRtcFlow(Role::kAnswerer, PeerId::FromRandom(), location_hint,
                        peer_id.GetId())) {
      return WebRtcSocketWrapper();
    }
  }

  NEARBY_LOG(WARNING, "Attempting to make a WebRTC connection to %s.",
             peer_id.GetId().c_str());
  Future<WebRtcSocketWrapper> socket_future;
  {
    MutexLock lock(&mutex_);
    socket_future = ListenForWebRtcSocketFuture(
        Role::kAnswerer, peer_id.GetId(),
        connecting_map_[peer_id.GetId()].connection_flow->GetDataChannel(),
        AcceptedConnectionCallback());
  }

  // The two devices have discovered each other, hence we have a timeout for
  // establishing the transport channel.
  // NOTE - We should not hold |mutex_| while waiting for the data channel since
  // it would block incoming signaling messages from being processed, resulting
  // in a timeout in creating the socket.
  ExceptionOr<WebRtcSocketWrapper> result =
      socket_future.Get(kDataChannelTimeout);
  if (result.ok()) {
    NEARBY_LOGS(WARNING) << "Succeeded to make WebRTC connection to "
                         << peer_id.GetId();
    return result.result();
  }

  NEARBY_LOGS(WARNING) << "Failed to make WebRTC connection to "
                       << peer_id.GetId();
  Disconnect(Role::kAnswerer, peer_id.GetId());
  return WebRtcSocketWrapper();
}

bool WebRtc::SetLocalSessionDescription(SessionDescriptionWrapper sdp,
                                        Role role,
                                        const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (!connection_info) return false;
  if (!connection_info->connection_flow->SetLocalSessionDescription(
          std::move(sdp))) {
    LogAndDisconnect(role, connection_id,
                     "Unable to set local session description");
    return false;
  }

  return true;
}

void WebRtc::StopAcceptingConnections(const std::string& service_id) {
  if (!IsAcceptingConnections(service_id)) {
    NEARBY_LOG(WARNING,
               "Skipped StopAcceptingConnections since we are not currently "
               "accepting WebRTC connections for %s",
               service_id.c_str());
    return;
  }

  {
    MutexLock lock(&mutex_);
    LogAndShutdownSignaling(Role::kOfferer, service_id,
                            "Invoked by StopAcceptingConnections.");
  }
  NEARBY_LOG(WARNING, "Stopped accepting WebRTC connections for %s",
             service_id.c_str());
}

Future<WebRtcSocketWrapper> WebRtc::ListenForWebRtcSocketFuture(
    const Role& role, const std::string& connection_id,
    Future<rtc::scoped_refptr<webrtc::DataChannelInterface>>
        data_channel_future,
    AcceptedConnectionCallback callback) {
  Future<WebRtcSocketWrapper> socket_future;
  auto data_channel_runnable = [this, role, connection_id, socket_future,
                                data_channel_future,
                                callback{std::move(callback)}]() mutable {
    // The overall timeout of creating the socket and data channel is controlled
    // by the caller of this function.
    ExceptionOr<rtc::scoped_refptr<webrtc::DataChannelInterface>> res =
        data_channel_future.Get();
    if (res.ok()) {
      WebRtcSocketWrapper wrapper =
          CreateWebRtcSocketWrapper(role, connection_id, res.result());
      callback.accepted_cb(wrapper);
      {
        MutexLock lock(&mutex_);
        ConnectionInfo* connection_info =
            GetConnectionInfo(role, connection_id);
        if (connection_info) {
          connection_info->socket = wrapper;
        }
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
    const Role& role, const std::string& connection_id,
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (data_channel == nullptr) {
    return WebRtcSocketWrapper();
  }

  auto socket = std::make_unique<WebRtcSocket>("WebRtcSocket", data_channel);
  socket->SetOnSocketClosedListener({[this, role, connection_id]() {
    OffloadFromSignalingThread(
        [this, role, connection_id]() { Disconnect(role, connection_id); });
  }});
  return WebRtcSocketWrapper(std::move(socket));
}

bool WebRtc::InitWebRtcFlow(const Role& role, const PeerId& self_id,
                            const LocationHint& location_hint,
                            const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (!connection_info) {
    NEARBY_LOGS(WARNING) << "Can not find matching connection info.";
    return false;
  }
  connection_info->self_id = self_id;

  if (connection_info->connection_flow) {
    LogAndShutdownSignaling(
        role, connection_id,
        "Tried to initialize WebRTC without shutting down the previous "
        "connection");
    return false;
  }

  if (connection_info->signaling_messenger) {
    LogAndShutdownSignaling(
        role, connection_id,
        "Tried to initialize WebRTC without shutting down signaling messenger");
    return false;
  }
  connection_info->signaling_messenger =
      medium_.GetSignalingMessenger(self_id.GetId(), location_hint);
  auto signaling_message_callback = std::bind(
      [this](ByteArray message, Role role, const std::string& connection_id) {
        OffloadFromSignalingThread([this, message{std::move(message)},
                                    role{role},
                                    connection_id{connection_id}]() {
          ProcessSignalingMessage(role, connection_id, message);
        });
      },
      std::placeholders::_1, role, connection_id);

  if (!connection_info->signaling_messenger->IsValid() ||
      !connection_info->signaling_messenger->StartReceivingMessages(
          signaling_message_callback)) {
    LogAndDisconnect(role, connection_id,
                     "Could not receive from signaling messenger.");
    return false;
  }

  if (role == Role::kAnswerer &&
      !connection_info->signaling_messenger->SendMessage(
          connection_info->peer_id.GetId(),
          webrtc_frames::EncodeReadyForSignalingPoke(self_id))) {
    LogAndDisconnect(Role::kAnswerer, connection_info->peer_id.GetId(),
                     absl::StrCat("Could not send signaling poke to peer ",
                                  connection_info->peer_id.GetId()));
    return false;
  }

  connection_info->connection_flow = ConnectionFlow::Create(
      GetLocalIceCandidateListener(role, connection_id),
      GetDataChannelListener(role, connection_id), medium_);
  if (!connection_info->connection_flow) {
    LogAndDisconnect(role, connection_id, "Failed to create connection flow");
    return false;
  }

  NEARBY_LOGS(WARNING) << "Succeeded to create connection flow for role:"
                       << role_names_[role]
                       << ", connection_id: " << connection_id;
  return true;
}

void WebRtc::OnLocalIceCandidate(
    const Role& role, const std::string& connection_id,
    const webrtc::IceCandidateInterface* local_ice_candidate) {
  ::location::nearby::mediums::IceCandidate ice_candidate =
      webrtc_frames::EncodeIceCandidate(*local_ice_candidate);

  OffloadFromSignalingThread([this, ice_candidate{std::move(ice_candidate)},
                              role{role}, connection_id{connection_id}]() {
    MutexLock lock(&mutex_);
    ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
    if (IsSignaling(role, connection_id)) {
      if (connection_info && connection_info->signaling_messenger) {
        NEARBY_LOG(WARNING, "Sending local ice candidates to %s",
                   connection_info->peer_id.GetId().c_str());
        connection_info->signaling_messenger->SendMessage(
            connection_info->peer_id.GetId(),
            webrtc_frames::EncodeIceCandidates(connection_info->self_id,
                                               {std::move(ice_candidate)}));
      } else {
        connection_info->pending_local_ice_candidates.push_back(
            std::move(ice_candidate));
      }
    } else {
      connection_info->pending_local_ice_candidates.push_back(
          std::move(ice_candidate));
    }
  });
}

LocalIceCandidateListener WebRtc::GetLocalIceCandidateListener(
    const Role& role, const std::string& connection_id) {
  return {std::bind(&WebRtc::OnLocalIceCandidate, this, role, connection_id,
                    std::placeholders::_1)};
}

void WebRtc::OnDataChannelClosed(const Role& role,
                                 const std::string& connection_id) {
  OffloadFromSignalingThread([this, role, connection_id]() {
    MutexLock lock(&mutex_);
    LogAndDisconnect(role, connection_id, "WebRTC data channel closed");
  });
}

void WebRtc::OnDataChannelMessageReceived(const Role& role,
                                          const std::string& connection_id,
                                          const ByteArray& message) {
  OffloadFromSignalingThread([this, role, connection_id, message]() {
    {
      MutexLock lock(&mutex_);
      ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
      if (!connection_info) return;
      if (!connection_info->socket.IsValid()) {
        LogAndDisconnect(role, connection_id,
                         "Received a data channel message without a socket");
        return;
      }
      connection_info->socket.NotifyDataChannelMsgReceived(message);
    }
  });
}

void WebRtc::OnDataChannelBufferedAmountChanged(
    const Role& role, const std::string& connection_id) {
  OffloadFromSignalingThread([this, role, connection_id]() {
    {
      MutexLock lock(&mutex_);
      ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
      if (!connection_info) return;
      if (!connection_info->socket.IsValid()) {
        LogAndDisconnect(role, connection_id,
                         "Data channel buffer changed without a socket");
        return;
      }
      connection_info->socket.NotifyDataChannelBufferedAmountChanged();
    }
  });
}

DataChannelListener WebRtc::GetDataChannelListener(
    const Role& role, const std::string& connection_id) {
  return {
      .data_channel_closed_cb =
          std::bind(&WebRtc::OnDataChannelClosed, this, role, connection_id),
      .data_channel_message_received_cb =
          std::bind(&WebRtc::OnDataChannelMessageReceived, this, role,
                    connection_id, std::placeholders::_1),
      .data_channel_buffered_amount_changed_cb =
          std::bind(&WebRtc::OnDataChannelBufferedAmountChanged, this, role,
                    connection_id),
  };
}

bool WebRtc::IsSignaling(const Role& role, const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (!connection_info) return false;
  return (connection_info->self_id.IsValid() &&
          connection_info->peer_id.IsValid());
}

void WebRtc::ProcessSignalingMessage(const Role& role,
                                     const std::string& connection_id,
                                     const ByteArray& message) {
  MutexLock lock(&mutex_);
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (!connection_info) {
    NEARBY_LOG(ERROR,
               "Could not find connection info for role: %s, connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
    return;
  }

  if (!connection_info->connection_flow) {
    LogAndDisconnect(role, connection_id,
                     "Received WebRTC frame before signaling was started");
    return;
  }

  location::nearby::mediums::WebRtcSignalingFrame frame;
  if (!frame.ParseFromString(std::string(message))) {
    LogAndDisconnect(role, connection_id, "Failed to parse signaling message");
    return;
  }

  if (!frame.has_sender_id()) {
    LogAndDisconnect(role, connection_id,
                     "Invalid WebRTC frame: Sender ID is missing");
    return;
  }

  if (frame.has_ready_for_signaling_poke() &&
      !connection_info->peer_id.IsValid()) {
    connection_info->peer_id = PeerId(frame.sender_id().id());
    NEARBY_LOG(WARNING, "Peer %s is ready for signaling",
               connection_info->peer_id.GetId().c_str());
  }

  if (!IsSignaling(role, connection_id)) {
    NEARBY_LOG(WARNING,
               "Ignoring WebRTC frame: we are not currently listening for "
               "signaling messages");
    return;
  }

  if (frame.sender_id().id() != connection_info->peer_id.GetId()) {
    NEARBY_LOG(
        WARNING,
        "Ignoring WebRTC frame: we are only listening for another peer.");
    return;
  }

  if (frame.has_ready_for_signaling_poke()) {
    NEARBY_LOG(WARNING,
               "Received ready-for-poke for role: %s connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
    SendOfferAndIceCandidatesToPeer(connection_id);
  } else if (frame.has_offer()) {
    NEARBY_LOG(WARNING, "Received offer for role: %s connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
    DCHECK(role == Role::kAnswerer);
    connection_info->connection_flow->OnOfferReceived(
        SessionDescriptionWrapper(webrtc_frames::DecodeOffer(frame).release()));
    SendAnswerToPeer(connection_id);
  } else if (frame.has_answer()) {
    NEARBY_LOG(WARNING, "Received answer for role: %s connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
    DCHECK(role == Role::kOfferer);
    connection_info->connection_flow->OnAnswerReceived(
        SessionDescriptionWrapper(
            webrtc_frames::DecodeAnswer(frame).release()));
  } else if (frame.has_ice_candidates()) {
    NEARBY_LOG(WARNING,
               "Received ice candidates for role: %s connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
    if (!connection_info->connection_flow->OnRemoteIceCandidatesReceived(
            webrtc_frames::DecodeIceCandidates(frame))) {
      LogAndDisconnect(role, connection_id,
                       "Could not add remote ice candidates.");
    }
  } else {
    NEARBY_LOG(WARNING,
               "Received unknown frame type for role: %s connection_id: %s",
               role_names_[role].c_str(), connection_id.c_str());
  }
}

void WebRtc::SendOfferAndIceCandidatesToPeer(const std::string& service_id) {
  ConnectionInfo* connection_info =
      GetConnectionInfo(Role::kOfferer, service_id);
  if (!connection_info) return;
  if (connection_info->pending_local_offer.Empty()) {
    LogAndDisconnect(
        Role::kOfferer, service_id,
        "Unable to send pending offer to remote peer: local offer not set");
    return;
  }

  NEARBY_LOG(WARNING, "Sending offer from %s", service_id.c_str());
  if (!connection_info->signaling_messenger->SendMessage(
          connection_info->peer_id.GetId(),
          connection_info->pending_local_offer)) {
    LogAndDisconnect(Role::kOfferer, service_id,
                     "Failed to send local offer via signaling messenger");
    return;
  }
  connection_info->pending_local_offer = ByteArray();

  if (!connection_info->pending_local_ice_candidates.empty()) {
    NEARBY_LOG(WARNING, "Sending local pending ice candidates from %s",
               service_id.c_str());
    connection_info->signaling_messenger->SendMessage(
        connection_info->peer_id.GetId(),
        webrtc_frames::EncodeIceCandidates(
            connection_info->self_id,
            std::move(connection_info->pending_local_ice_candidates)));
  }
}

void WebRtc::SendAnswerToPeer(const std::string& peer_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(Role::kAnswerer, peer_id);
  if (!connection_info) return;
  SessionDescriptionWrapper answer =
      connection_info->connection_flow->CreateAnswer();
  ByteArray answer_message(
      webrtc_frames::EncodeAnswer(connection_info->self_id, answer.GetSdp()));

  if (!SetLocalSessionDescription(std::move(answer), Role::kAnswerer, peer_id))
    return;

  NEARBY_LOGS(WARNING) << "Sending answer to peer " << peer_id;
  if (!connection_info->signaling_messenger->SendMessage(
          connection_info->peer_id.GetId(), answer_message)) {
    LogAndDisconnect(Role::kAnswerer, peer_id,
                     "Failed to send local answer via signaling messenger");
    return;
  }
}

void WebRtc::LogAndDisconnect(const Role& role,
                              const std::string& connection_id,
                              const std::string& error_message) {
  NEARBY_LOG(WARNING,
             "Disconnecting WebRTC role: %d, connection id: %s, msg: %s", role,
             connection_id.c_str(), error_message.c_str());
  DisconnectLocked(role, connection_id);
}

void WebRtc::LogAndShutdownSignaling(const Role& role,
                                     const std::string& connection_id,
                                     const std::string& error_message) {
  NEARBY_LOG(WARNING,
             "Stopping WebRTC role: %s, connection id: %s, msg: %s:\n%s",
             role_names_[role].c_str(), connection_id.c_str(),
             error_message.c_str(), InternalStatesToString().c_str());
  ShutdownSignaling(role, connection_id);
}

void WebRtc::ShutdownSignaling(const Role& role,
                               const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (!connection_info) {
    return;
  }

  connection_info->self_id = PeerId();
  connection_info->peer_id = PeerId();
  connection_info->pending_local_offer = ByteArray();
  connection_info->pending_local_ice_candidates.clear();

  if (connection_info->restart_receive_messages_alarm.IsValid()) {
    connection_info->restart_receive_messages_alarm.Cancel();
    connection_info->restart_receive_messages_alarm = CancelableAlarm();
  }

  if (connection_info->signaling_messenger) {
    connection_info->signaling_messenger->StopReceivingMessages();
    connection_info->signaling_messenger.reset();
  }

  if (!connection_info->socket.IsValid())
    ShutdownIceCandidateCollection(role, connection_id);
}

void WebRtc::Disconnect(const Role& role, const std::string& connection_id) {
  MutexLock lock(&mutex_);
  DisconnectLocked(role, connection_id);
}

void WebRtc::DisconnectLocked(const Role& role,
                              const std::string& connection_id) {
  NEARBY_LOGS(WARNING) << "Disconnecting role: " << role_names_[role]
                       << " connection_id: " << connection_id << ":\n"
                       << InternalStatesToString();
  ShutdownSignaling(role, connection_id);
  ShutdownWebRtcSocket(role, connection_id);
  ShutdownIceCandidateCollection(role, connection_id);

  if (role == Role::kOfferer && accepting_map_.contains(connection_id)) {
    accepting_map_.erase(connection_id);
  } else if (role == Role::kAnswerer &&
             connecting_map_.contains(connection_id)) {
    connecting_map_.erase(connection_id);
  }
}

void WebRtc::ShutdownWebRtcSocket(const Role& role,
                                  const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (connection_info && connection_info->socket.IsValid()) {
    connection_info->socket.Close();
    connection_info->socket = WebRtcSocketWrapper();
  }
}

void WebRtc::ShutdownIceCandidateCollection(const Role& role,
                                            const std::string& connection_id) {
  ConnectionInfo* connection_info = GetConnectionInfo(role, connection_id);
  if (connection_info && connection_info->connection_flow) {
    connection_info->connection_flow->Close();
    connection_info->connection_flow.reset();
  }
}

void WebRtc::OffloadFromSignalingThread(Runnable runnable) {
  single_thread_executor_.Execute(std::move(runnable));
}

void WebRtc::RestartReceiveMessages(const LocationHint& location_hint,
                                    const std::string& service_id) {
  if (!IsAcceptingConnections(service_id)) {
    NEARBY_LOG(WARNING,
               "Skipping restart since we are not accepting connections.");
    return;
  }

  NEARBY_LOG(WARNING, "Restarting listening for receiving signaling messages.");
  {
    MutexLock lock(&mutex_);
    ConnectionInfo* connection_info =
        GetConnectionInfo(Role::kOfferer, service_id);
    if (!connection_info) {
      NEARBY_LOG(ERROR,
                 "Can't find connection info in RestartReceiveMessages for %s",
                 service_id.c_str());
      return;
    }
    connection_info->signaling_messenger->StopReceivingMessages();

    connection_info->signaling_messenger = medium_.GetSignalingMessenger(
        connection_info->self_id.GetId(), location_hint);

    auto signaling_message_callback = std::bind(
        [this](ByteArray message, const Role& role,
               const std::string& connection_id) {
          OffloadFromSignalingThread([this, message{std::move(message)},
                                      role{role},
                                      connection_id{connection_id}]() {
            ProcessSignalingMessage(role, connection_id, message);
          });
        },
        std::placeholders::_1, Role::kOfferer, service_id);

    if (!connection_info->signaling_messenger->IsValid() ||
        !connection_info->signaling_messenger->StartReceivingMessages(
            signaling_message_callback)) {
      DisconnectLocked(Role::kOfferer, service_id);
    }
  }
}

WebRtc::ConnectionInfo* WebRtc::GetConnectionInfo(
    const Role& role, const std::string& connection_id) {
  if (role == Role::kOfferer && accepting_map_.contains(connection_id)) {
    return &accepting_map_[connection_id];
  } else if (role == Role::kAnswerer &&
             connecting_map_.contains(connection_id)) {
    return &connecting_map_[connection_id];
  }
  return nullptr;
}

std::string WebRtc::ConnectionInfo::ToString() const {
  std::ostringstream result;
  result << (connection_flow == nullptr ? "connection_flow is null, "
                                        : "connection_flow is valid, ");
  result << (signaling_messenger == nullptr ? "signaling_messenger is null, "
                                            : "signaling_messenger is valid, ");
  result << (socket.IsValid() ? "socket is valid, " : "socket is not valid, ");
  result << "remote peer_id: " << peer_id.GetId() << ", ";
  result << "self peer_id: " << self_id.GetId();
  return result.str();
}

std::string WebRtc::InternalStatesToString() {
  std::ostringstream map_values;
  map_values << "connecting map size: " << connecting_map_.size()
             << ", accepting map size: " << accepting_map_.size() << "\n";
  for (auto& item : connecting_map_) {
    map_values << "connecting " << item.first << ": " << item.second.ToString()
               << "\n";
  }
  for (auto& item : accepting_map_) {
    map_values << "accepting " << item.first << ": " << item.second.ToString()
               << "\n";
  }
  return map_values.str();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
