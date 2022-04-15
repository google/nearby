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

#include "connections/implementation/mediums/webrtc.h"

#include <functional>
#include <memory>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/webrtc/session_description_wrapper.h"
#include "connections/implementation/mediums/webrtc/signaling_frames.h"
#include "connections/implementation/mediums/webrtc_socket.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/listeners.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "proto/mediums/web_rtc_signaling_frames.pb.h"
#include "webrtc/api/jsep.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

// The maximum amount of time to wait to connect to a data channel via WebRTC.
constexpr absl::Duration kDataChannelTimeout = absl::Seconds(10);

// Delay between restarting signaling messenger to receive messages.
constexpr absl::Duration kRestartReceiveMessagesDuration = absl::Seconds(60);

}  // namespace

WebRtc::WebRtc() = default;

WebRtc::~WebRtc() {
  // This ensures that all pending callbacks are run before we reset the medium
  // and we are not accepting new runnables.
  single_thread_executor_.Shutdown();

  // Stop accepting all connections
  absl::flat_hash_set<std::string> service_ids;
  for (auto& item : accepting_connections_info_) {
    service_ids.emplace(item.first);
  }
  for (const auto& service_id : service_ids) {
    StopAcceptingConnections(service_id);
  }
}

const std::string WebRtc::GetDefaultCountryCode() {
  return medium_.GetDefaultCountryCode();
}

bool WebRtc::IsAvailable() { return medium_.IsValid(); }

bool WebRtc::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WebRtc::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return accepting_connections_info_.contains(service_id);
}

bool WebRtc::StartAcceptingConnections(const std::string& service_id,
                                       const WebrtcPeerId& self_peer_id,
                                       const LocationHint& location_hint,
                                       AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);
  if (!IsAvailable()) {
    NEARBY_LOG(WARNING,
               "Cannot start accepting WebRTC connections because WebRTC is "
               "not available.");
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOG(WARNING,
               "Cannot start accepting WebRTC connections because service %s "
               "is already accepting WebRTC connections.",
               service_id.c_str());
    return false;
  }

  // We'll track our state here, so that we're separated from the other services
  // who may be also using WebRTC.
  AcceptingConnectionsInfo info = AcceptingConnectionsInfo();
  info.self_peer_id = self_peer_id;
  info.accepted_connection_callback = callback;

  // Create a new SignalingMessenger so that we can communicate w/ Tachyon.
  info.signaling_messenger =
      medium_.GetSignalingMessenger(self_peer_id.GetId(), location_hint);
  if (!info.signaling_messenger->IsValid()) {
    return false;
  }

  // This registers ourselves w/ Tachyon, creating a room from the PeerId.
  // This allows a remote device to message us over Tachyon.
  if (!info.signaling_messenger->StartReceivingMessages(
          absl::bind_front(&WebRtc::OnSignalingMessage, this, service_id),
          absl::bind_front(&WebRtc::OnSignalingComplete, this, service_id))) {
    info.signaling_messenger.reset();
    return false;
  }

  // We'll automatically disconnect from Tachyon after 60sec. When this alarm
  // fires, we'll recreate our room so we continue to receive messages.
  info.restart_tachyon_receive_messages_alarm = CancelableAlarm(
      "restart_receiving_messages_webrtc",
      std::bind(&WebRtc::ProcessRestartTachyonReceiveMessages, this,
                service_id),
      kRestartReceiveMessagesDuration, &single_thread_executor_);

  // Now that we're set up to receive messages, we'll save our state and return
  // a successful result.
  accepting_connections_info_.emplace(service_id, std::move(info));
  NEARBY_LOG(INFO,
             "Started listening for WebRTC connections as %s on service %s",
             self_peer_id.GetId().c_str(), service_id.c_str());
  return true;
}

void WebRtc::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  if (!IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOG(WARNING,
               "Cannot stop accepting WebRTC connections because service %s "
               "is not accepting WebRTC connections.",
               service_id.c_str());
    return;
  }

  // Grab our info from the map.
  auto& info = accepting_connections_info_.find(service_id)->second;

  // Stop receiving messages from Tachyon.
  info.signaling_messenger->StopReceivingMessages();
  info.signaling_messenger.reset();

  // Cancel the scheduled alarm.
  if (info.restart_tachyon_receive_messages_alarm.IsValid()) {
    info.restart_tachyon_receive_messages_alarm.Cancel();
    info.restart_tachyon_receive_messages_alarm = CancelableAlarm();
  }

  // If we had any in-progress connections that haven't materialized into full
  // DataChannels yet, it's time to shut them down since they can't reach us
  // anymore.
  absl::flat_hash_set<std::string> peer_ids;
  for (auto& item : connection_flows_) {
    peer_ids.emplace(item.first);
  }
  for (const auto& peer_id : peer_ids) {
    const auto& entry = connection_flows_.find(peer_id);
    // Skip outgoing connections in this step. Start/StopAcceptingConnections
    // only deals with incoming connections.
    if (requesting_connections_info_.contains(peer_id)) {
      continue;
    }

    // Skip fully connected connections in this step. If the connection was
    // formed while we were accepting connections, then it will stay alive until
    // it's explicitly closed.
    if (!entry->second->CloseIfNotConnected()) {
      continue;
    }

    connection_flows_.erase(peer_id);
  }

  // Clean up our state. We're now no longer listening for connections.
  accepting_connections_info_.erase(service_id);
  NEARBY_LOG(INFO, "Stopped listening for WebRTC connections for service %s",
             service_id.c_str());
}

WebRtcSocketWrapper WebRtc::Connect(const std::string& service_id,
                                    const WebrtcPeerId& remote_peer_id,
                                    const LocationHint& location_hint,
                                    CancellationFlag* cancellation_flag) {
  for (int attempts_count = 0; attempts_count < kConnectAttemptsLimit;
       attempts_count++) {
    auto wrapper_result = AttemptToConnect(service_id, remote_peer_id,
                                           location_hint, cancellation_flag);
    if (wrapper_result.IsValid()) {
      return wrapper_result;
    }
  }
  return WebRtcSocketWrapper();
}

WebRtcSocketWrapper WebRtc::AttemptToConnect(
    const std::string& service_id, const WebrtcPeerId& remote_peer_id,
    const LocationHint& location_hint, CancellationFlag* cancellation_flag) {
  ConnectionRequestInfo info = ConnectionRequestInfo();
  info.self_peer_id = WebrtcPeerId::FromRandom();
  Future<WebRtcSocketWrapper> socket_future = info.socket_future;

  {
    MutexLock lock(&mutex_);
    if (!IsAvailable()) {
      NEARBY_LOG(
          WARNING,
          "Cannot connect to WebRTC peer %s because WebRTC is not available.",
          remote_peer_id.GetId().c_str());
      return WebRtcSocketWrapper();
    }

    if (cancellation_flag->Cancelled()) {
      NEARBY_LOGS(INFO) << "Cannot connect with WebRtc due to cancel.";
      return WebRtcSocketWrapper();
    }

    // Create a new ConnectionFlow for this connection attempt.
    std::unique_ptr<ConnectionFlow> connection_flow =
        CreateConnectionFlow(service_id, remote_peer_id);
    if (!connection_flow) {
      NEARBY_LOG(
          INFO,
          "Cannot connect to WebRTC peer %s because we failed to create a "
          "ConnectionFlow.",
          remote_peer_id.GetId().c_str());
      return WebRtcSocketWrapper();
    }

    // Create a new SignalingMessenger so that we can communicate over Tachyon.
    info.signaling_messenger =
        medium_.GetSignalingMessenger(info.self_peer_id.GetId(), location_hint);
    if (!info.signaling_messenger->IsValid()) {
      NEARBY_LOG(
          INFO,
          "Cannot connect to WebRTC peer %s because we failed to create a "
          "SignalingMessenger.",
          remote_peer_id.GetId().c_str());
      return WebRtcSocketWrapper();
    }

    // This registers ourselves w/ Tachyon, creating a room from the PeerId.
    // This allows a remote device to message us over Tachyon.
    auto signaling_complete_callback = [socket_future](bool success) mutable {
      if (!success) {
        socket_future.SetException({Exception::kFailed});
      }
    };
    if (!info.signaling_messenger->StartReceivingMessages(
            absl::bind_front(&WebRtc::OnSignalingMessage, this, service_id),
            signaling_complete_callback)) {
      NEARBY_LOG(INFO,
                 "Cannot connect to WebRTC peer %s because we failed to start "
                 "receiving messages over Tachyon.",
                 remote_peer_id.GetId().c_str());
      info.signaling_messenger.reset();
      return WebRtcSocketWrapper();
    }

    // Poke the remote device. This will cause them to send us an Offer.
    if (!info.signaling_messenger->SendMessage(
            remote_peer_id.GetId(),
            webrtc_frames::EncodeReadyForSignalingPoke(info.self_peer_id))) {
      NEARBY_LOG(INFO,
                 "Cannot connect to WebRTC peer %s because we failed to poke "
                 "the peer over Tachyon.",
                 remote_peer_id.GetId().c_str());
      info.signaling_messenger.reset();
      return WebRtcSocketWrapper();
    }

    // Create a new ConnectionRequest entry. This map will be used later to look
    // up state as we negotiate the connection over Tachyon.
    requesting_connections_info_.emplace(remote_peer_id.GetId(),
                                         std::move(info));
    connection_flows_.emplace(remote_peer_id.GetId(),
                              std::move(connection_flow));
  }

  // Wait for the connection to go through. Don't hold the mutex here so that
  // we're not blocking necessary operations.
  ExceptionOr<WebRtcSocketWrapper> socket_result =
      socket_future.Get(kDataChannelTimeout);

  {
    MutexLock lock(&mutex_);

    // Reclaim our info, since we had released ownership while talking to
    // Tachyon.
    auto& info =
        requesting_connections_info_.find(remote_peer_id.GetId())->second;

    // Verify that the connection went through.
    if (!socket_result.ok()) {
      NEARBY_LOG(INFO, "Failed to connect to WebRTC peer %s.",
                 remote_peer_id.GetId().c_str());
      RemoveConnectionFlow(remote_peer_id);
      info.signaling_messenger.reset();
      requesting_connections_info_.erase(remote_peer_id.GetId());
      return WebRtcSocketWrapper();
    }

    // Clean up our ConnectionRequest.
    info.signaling_messenger.reset();
    requesting_connections_info_.erase(remote_peer_id.GetId());

    // Return the result.
    return socket_result.GetResult();
  }
}

void WebRtc::ProcessLocalIceCandidate(
    const std::string& service_id, const WebrtcPeerId& remote_peer_id,
    const ::location::nearby::mediums::IceCandidate ice_candidate) {
  MutexLock lock(&mutex_);

  // Check first if we have an outgoing request w/ this peer. As this request is
  // tied to a specific peer, it takes precedence.
  const auto& connection_request_entry =
      requesting_connections_info_.find(remote_peer_id.GetId());
  if (connection_request_entry != requesting_connections_info_.end()) {
    // Pass the ice candidate to the remote side.
    if (!connection_request_entry->second.signaling_messenger->SendMessage(
            remote_peer_id.GetId(),
            webrtc_frames::EncodeIceCandidates(
                connection_request_entry->second.self_peer_id,
                {ice_candidate}))) {
      NEARBY_LOG(INFO, "Failed to send ice candidate to %s.",
                 remote_peer_id.GetId().c_str());
    }

    NEARBY_LOG(INFO, "Sent ice candidate to %s.",
               remote_peer_id.GetId().c_str());
    return;
  }

  // Check next if we're expecting incoming connection requests.
  const auto& accepting_connection_entry =
      accepting_connections_info_.find(service_id);
  if (accepting_connection_entry != accepting_connections_info_.end()) {
    // Pass the ice candidate to the remote side.
    // TODO(xlythe) Consider not blocking here, since this can eat into the
    // connection time
    if (!accepting_connection_entry->second.signaling_messenger->SendMessage(
            remote_peer_id.GetId(),
            webrtc_frames::EncodeIceCandidates(
                accepting_connection_entry->second.self_peer_id,
                {ice_candidate}))) {
      NEARBY_LOG(INFO, "Failed to send ice candidate to %s.",
                 remote_peer_id.GetId().c_str());
    }

    NEARBY_LOG(INFO, "Sent ice candidate to %s.",
               remote_peer_id.GetId().c_str());
    return;
  }

  NEARBY_LOG(INFO,
             "Skipping restart listening for tachyon inbox messages since we "
             "are not accepting connections for service %s.",
             service_id.c_str());
}

void WebRtc::OnSignalingMessage(const std::string& service_id,
                                const ByteArray& message) {
  OffloadFromThread("rtc-on-signaling-message", [this, service_id, message]() {
    ProcessTachyonInboxMessage(service_id, message);
  });
}

void WebRtc::OnSignalingComplete(const std::string& service_id, bool success) {
  NEARBY_LOG(INFO, "Signaling completed with status: %d.", success);
  if (success) {
    return;
  }

  OffloadFromThread("rtc-on-signaling-complete", [this, service_id]() {
    MutexLock lock(&mutex_);
    const auto& info_entry = accepting_connections_info_.find(service_id);
    if (info_entry == accepting_connections_info_.end()) {
      return;
    }

    if (info_entry->second.restart_accept_connections_count <
        kRestartAcceptConnectionsLimit) {
      ++info_entry->second.restart_accept_connections_count;
    } else {
      return;
    }

    RestartTachyonReceiveMessages(service_id);
  });
}

void WebRtc::ProcessTachyonInboxMessage(const std::string& service_id,
                                        const ByteArray& message) {
  MutexLock lock(&mutex_);

  // Attempt to parse the incoming message as a WebRtcSignalingFrame.
  location::nearby::mediums::WebRtcSignalingFrame frame;
  if (!frame.ParseFromString(std::string(message))) {
    NEARBY_LOG(WARNING, "Failed to parse signaling message.");
    return;
  }

  // Ensure that the frame is valid (no missing fields).
  if (!frame.has_sender_id()) {
    NEARBY_LOG(WARNING, "Invalid WebRTC frame: Sender ID is missing.");
    return;
  }
  WebrtcPeerId remote_peer_id = WebrtcPeerId(frame.sender_id().id());

  // Depending on the message type, we'll respond as appropriate.
  if (requesting_connections_info_.contains(remote_peer_id.GetId())) {
    // This is from a peer we have an outgoing connection request with, so we'll
    // only process the Answer path.
    if (frame.has_offer()) {
      ReceiveOffer(remote_peer_id,
                   SessionDescriptionWrapper(
                       webrtc_frames::DecodeOffer(frame).release()));
      SendAnswer(remote_peer_id);
    } else if (frame.has_ice_candidates()) {
      ReceiveIceCandidates(remote_peer_id,
                           webrtc_frames::DecodeIceCandidates(frame));
    } else {
      NEARBY_LOG(INFO, "Received unknown WebRTC frame: ignoring.");
    }
  } else if (IsAcceptingConnectionsLocked(service_id)) {
    // We don't have an outgoing connection request with this peer, but we are
    // accepting incoming requests so we'll only process the Offer path.
    if (frame.has_ready_for_signaling_poke()) {
      SendOffer(service_id, remote_peer_id);
    } else if (frame.has_answer()) {
      ReceiveAnswer(remote_peer_id,
                    SessionDescriptionWrapper(
                        webrtc_frames::DecodeAnswer(frame).release()));
    } else if (frame.has_ice_candidates()) {
      ReceiveIceCandidates(remote_peer_id,
                           webrtc_frames::DecodeIceCandidates(frame));
    } else {
      NEARBY_LOG(INFO, "Received unknown WebRTC frame: ignoring.");
    }
  } else {
    NEARBY_LOG(
        INFO,
        "Ignoring Tachyon message since we are not accepting connections.");
  }
}

void WebRtc::SendOffer(const std::string& service_id,
                       const WebrtcPeerId& remote_peer_id) {
  std::unique_ptr<ConnectionFlow> connection_flow =
      CreateConnectionFlow(service_id, remote_peer_id);
  if (!connection_flow) {
    NEARBY_LOG(INFO,
               "Unable to send offer. Failed to create a ConnectionFlow.");
    return;
  }

  SessionDescriptionWrapper offer = connection_flow->CreateOffer();
  if (!offer.IsValid()) {
    NEARBY_LOG(INFO,
               "Unable to send offer. Failed to create our offer locally.");
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  const webrtc::SessionDescriptionInterface& sdp = offer.GetSdp();
  if (!connection_flow->SetLocalSessionDescription(offer)) {
    NEARBY_LOG(INFO,
               "Unable to send offer. Failed to register our offer locally.");
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  // Grab our info from the map.
  auto& info = accepting_connections_info_.find(service_id)->second;

  // Pass the offer to the remote side.
  if (!info.signaling_messenger->SendMessage(
          remote_peer_id.GetId(),
          webrtc_frames::EncodeOffer(info.self_peer_id, sdp))) {
    NEARBY_LOG(INFO,
               "Unable to send offer. Failed to write the offer to the remote "
               "peer %s.",
               remote_peer_id.GetId().c_str());
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  // Store the ConnectionFlow so that other methods can use it later.
  connection_flows_.emplace(remote_peer_id.GetId(), std::move(connection_flow));
  NEARBY_LOG(INFO, "Sent offer to %s.", remote_peer_id.GetId().c_str());
}

void WebRtc::ReceiveOffer(const WebrtcPeerId& remote_peer_id,
                          SessionDescriptionWrapper offer) {
  const auto& entry = connection_flows_.find(remote_peer_id.GetId());
  if (entry == connection_flows_.end()) {
    NEARBY_LOG(INFO,
               "Unable to receive offer. Failed to create a ConnectionFlow.");
    return;
  }

  if (!entry->second->OnOfferReceived(offer)) {
    NEARBY_LOG(INFO, "Unable to receive offer. Failed to process the offer.");
    RemoveConnectionFlow(remote_peer_id);
  }
}

void WebRtc::SendAnswer(const WebrtcPeerId& remote_peer_id) {
  const auto& entry = connection_flows_.find(remote_peer_id.GetId());
  if (entry == connection_flows_.end()) {
    NEARBY_LOG(INFO,
               "Unable to send answer. Failed to create a ConnectionFlow.");
    return;
  }

  SessionDescriptionWrapper answer = entry->second->CreateAnswer();
  if (!answer.IsValid()) {
    NEARBY_LOG(INFO,
               "Unable to send answer. Failed to create our answer locally.");
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  const webrtc::SessionDescriptionInterface& sdp = answer.GetSdp();
  if (!entry->second->SetLocalSessionDescription(answer)) {
    NEARBY_LOG(INFO,
               "Unable to send answer. Failed to register our answer locally.");
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  // Grab our info from the map.
  const auto& connection_request_entry =
      requesting_connections_info_.find(remote_peer_id.GetId());
  if (connection_request_entry == requesting_connections_info_.end()) {
    NEARBY_LOG(INFO,
               "Unable to send answer. Failed to find an outgoing connection "
               "request.");
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  // Pass the answer to the remote side.
  if (!connection_request_entry->second.signaling_messenger->SendMessage(
          remote_peer_id.GetId(),
          webrtc_frames::EncodeAnswer(
              connection_request_entry->second.self_peer_id, sdp))) {
    NEARBY_LOG(
        INFO,
        "Unable to send answer. Failed to write the answer to the remote "
        "peer %s.",
        remote_peer_id.GetId().c_str());
    RemoveConnectionFlow(remote_peer_id);
    return;
  }

  NEARBY_LOG(INFO, "Sent answer to %s.", remote_peer_id.GetId().c_str());
}

void WebRtc::ReceiveAnswer(const WebrtcPeerId& remote_peer_id,
                           SessionDescriptionWrapper answer) {
  const auto& entry = connection_flows_.find(remote_peer_id.GetId());
  if (entry == connection_flows_.end()) {
    NEARBY_LOG(INFO,
               "Unable to receive answer. Failed to create a ConnectionFlow.");
    return;
  }

  if (!entry->second->OnAnswerReceived(answer)) {
    NEARBY_LOG(INFO, "Unable to receive answer. Failed to process the answer.");
    RemoveConnectionFlow(remote_peer_id);
  }
}

void WebRtc::ReceiveIceCandidates(
    const WebrtcPeerId& remote_peer_id,
    std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
        ice_candidates) {
  const auto& entry = connection_flows_.find(remote_peer_id.GetId());
  if (entry == connection_flows_.end()) {
    NEARBY_LOG(
        INFO,
        "Unable to receive ice candidates. Failed to create a ConnectionFlow.");
    return;
  }

  entry->second->OnRemoteIceCandidatesReceived(std::move(ice_candidates));
}

void WebRtc::ProcessRestartTachyonReceiveMessages(
    const std::string& service_id) {
  MutexLock lock(&mutex_);
  RestartTachyonReceiveMessages(service_id);
}

void WebRtc::RestartTachyonReceiveMessages(const std::string& service_id) {
  if (!IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOG(INFO,
               "Skipping restart listening for tachyon inbox messages since we "
               "are not accepting connections for service %s.",
               service_id.c_str());
    return;
  }

  // Grab our info from the map.
  auto& info = accepting_connections_info_.find(service_id)->second;

  // Ensure we've disconnected from Tachyon.
  info.signaling_messenger->StopReceivingMessages();

  // Attempt to re-register.
  if (!info.signaling_messenger->StartReceivingMessages(
          absl::bind_front(&WebRtc::OnSignalingMessage, this, service_id),
          absl::bind_front(&WebRtc::OnSignalingComplete, this, service_id))) {
    NEARBY_LOG(WARNING,
               "Failed to restart listening for tachyon inbox messages for "
               "service %s since we failed to reach Tachyon.",
               service_id.c_str());
    return;
  }

  NEARBY_LOG(INFO,
             "Successfully restarted listening for tachyon inbox messages on "
             "service %s.",
             service_id.c_str());
}

void WebRtc::ProcessDataChannelOpen(const std::string& service_id,
                                    const WebrtcPeerId& remote_peer_id,
                                    WebRtcSocketWrapper socket_wrapper) {
  MutexLock lock(&mutex_);

  // Notify the client of the newly formed socket.
  const auto& connection_request_entry =
      requesting_connections_info_.find(remote_peer_id.GetId());
  if (connection_request_entry != requesting_connections_info_.end()) {
    connection_request_entry->second.socket_future.Set(socket_wrapper);
    return;
  }

  const auto& accepting_connection_entry =
      accepting_connections_info_.find(service_id);
  if (accepting_connection_entry != accepting_connections_info_.end()) {
    accepting_connection_entry->second.accepted_connection_callback.accepted_cb(
        service_id, socket_wrapper);
    return;
  }

  // No one to handle the newly created DataChannel, so we'll just close it.
  socket_wrapper.Close();
  NEARBY_LOG(INFO,
             "Ignoring new DataChannel because we "
             "are not accepting connections for service %s.",
             service_id.c_str());
}

void WebRtc::ProcessDataChannelClosed(const WebrtcPeerId& remote_peer_id) {
  MutexLock lock(&mutex_);
  NEARBY_LOG(INFO,
             "Data channel has closed, removing connection flow for peer %s.",
             remote_peer_id.GetId().c_str());

  RemoveConnectionFlow(remote_peer_id);
}

std::unique_ptr<ConnectionFlow> WebRtc::CreateConnectionFlow(
    const std::string& service_id, const WebrtcPeerId& remote_peer_id) {
  RemoveConnectionFlow(remote_peer_id);

  return ConnectionFlow::Create(
      {.local_ice_candidate_found_cb =
           {[this, service_id, remote_peer_id](
                const webrtc::IceCandidateInterface* ice_candidate) {
             // Note: We need to encode the ice candidate here, before we jump
             // off the thread. Otherwise, it gets destroyed and we can't read
             // it later.
             ::location::nearby::mediums::IceCandidate encoded_ice_candidate =
                 webrtc_frames::EncodeIceCandidate(*ice_candidate);
             OffloadFromThread(
                 "rtc-ice-candidates",
                 [this, service_id, remote_peer_id, encoded_ice_candidate]() {
                   ProcessLocalIceCandidate(service_id, remote_peer_id,
                                            encoded_ice_candidate);
                 });
           }}},
      {
          .data_channel_open_cb = {[this, service_id, remote_peer_id](
                                       WebRtcSocketWrapper socket_wrapper) {
            OffloadFromThread(
                "rtc-channel-created",
                [this, service_id, remote_peer_id, socket_wrapper]() {
                  ProcessDataChannelOpen(service_id, remote_peer_id,
                                         socket_wrapper);
                });
          }},
          .data_channel_closed_cb = {[this, remote_peer_id]() {
            OffloadFromThread("rtc-channel-closed", [this, remote_peer_id]() {
              ProcessDataChannelClosed(remote_peer_id);
            });
          }},
      },
      medium_);
}

void WebRtc::RemoveConnectionFlow(const WebrtcPeerId& remote_peer_id) {
  if (!connection_flows_.erase(remote_peer_id.GetId())) {
    return;
  }

  // If we had an outgoing connection request w/ this peer, report the failure
  // to the future that's being waited on.
  const auto& connection_request_entry =
      requesting_connections_info_.find(remote_peer_id.GetId());
  if (connection_request_entry != requesting_connections_info_.end()) {
    connection_request_entry->second.socket_future.SetException(
        {Exception::kFailed});
  }
}

void WebRtc::OffloadFromThread(const std::string& name, Runnable runnable) {
  single_thread_executor_.Execute(name, std::move(runnable));
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
