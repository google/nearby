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

#include "connections/implementation/webrtc_bwu_handler.h"

#include <string>

#include "absl/functional/bind_front.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/mediums/webrtc_peer_id.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/webrtc_endpoint_channel.h"

namespace location {
namespace nearby {
namespace connections {

WebrtcBwuHandler::WebrtcBwuHandler(Mediums& mediums,
                                   EndpointChannelManager& channel_manager,
                                   BwuNotifications notifications)
    : BaseBwuHandler(channel_manager, std::move(notifications)),
      mediums_(mediums) {}

void WebrtcBwuHandler::Revert() {
  for (const auto& service_id : active_service_ids_) {
    webrtc_.StopAcceptingConnections(service_id);
  }
  active_service_ids_.clear();

  NEARBY_LOG(INFO, "WebrtcBwuHandler successfully reverted state.");
}

// Accept Connection Callback.
// Notifies that the remote party called WebRtc::Connect()
// for this socket.
void WebrtcBwuHandler::OnIncomingWebrtcConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    mediums::WebRtcSocketWrapper socket) {
  std::string service_id = Utils::UnwrapUpgradeServiceId(upgrade_service_id);
  auto channel = std::make_unique<WebRtcEndpointChannel>(service_id, socket);
  auto webrtc_socket =
      std::make_unique<WebrtcIncomingSocket>(service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{std::move(webrtc_socket),
                                   std::move(channel)});

  bwu_notifications_.incoming_connection_cb(client, std::move(connection));
}

// Called by BWU initiator. Set up WebRTC upgraded medium for this endpoint,
// and returns a upgrade path info (PeerId, LocationHint) for remote party to
// perform discovery.
ByteArray WebrtcBwuHandler::InitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id) {
  // Use wrapped service ID to avoid have the same ID with the one for
  // startAdvertising. Otherwise, the listening request would be ignored because
  // the medium already start accepting the connection because the client not
  // stop the advertising yet.
  std::string upgrade_service_id = Utils::WrapUpgradeServiceId(service_id);

  LocationHint location_hint =
      Utils::BuildLocationHint(webrtc_.GetDefaultCountryCode());

  mediums::WebrtcPeerId self_id{mediums::WebrtcPeerId::FromRandom()};
  if (!webrtc_.IsAcceptingConnections(service_id)) {
    if (!webrtc_.StartAcceptingConnections(
            upgrade_service_id, self_id, location_hint,
            {
                .accepted_cb = absl::bind_front(
                    &WebrtcBwuHandler::OnIncomingWebrtcConnection, this, client,
                    upgrade_service_id),
            })) {
      NEARBY_LOG(ERROR,
                 "WebRtcBwuHandler couldn't initiate the WEB_RTC upgrade for "
                 "endpoint %s because it failed to start listening for "
                 "incoming WebRTC connections.",
                 endpoint_id.c_str());
      return {};
    }
    NEARBY_LOG(INFO,
               "WebRtcBwuHandler successfully started listening for incoming "
               "WebRTC connections while upgrading endpoint %s",
               endpoint_id.c_str());
  }

  // cache service ID to revert
  active_service_ids_.emplace(upgrade_service_id);

  return parser::ForBwuWebrtcPathAvailable(self_id.GetId(), location_hint);
}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WebRTC using this info.
std::unique_ptr<EndpointChannel>
WebrtcBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  const UpgradePathInfo::WebRtcCredentials& web_rtc_credentials =
      upgrade_path_info.web_rtc_credentials();
  mediums::WebrtcPeerId peer_id(web_rtc_credentials.peer_id());

  LocationHint location_hint;
  location_hint.set_format(LocationStandard::UNKNOWN);
  if (web_rtc_credentials.has_location_hint()) {
    location_hint = web_rtc_credentials.location_hint();
  }
  NEARBY_LOG(INFO,
             "WebRtcBwuHandler is attempting to connect to remote peer %s, "
             "location hint %s",
             peer_id.GetId().c_str(), location_hint.DebugString().c_str());

  mediums::WebRtcSocketWrapper socket =
      webrtc_.Connect(service_id, peer_id, location_hint,
                      client->GetCancellationFlag(endpoint_id));
  if (!socket.IsValid()) {
    NEARBY_LOG(ERROR,
               "WebRtcBwuHandler failed to connect to remote peer (%s) on "
               "endpoint %s, aborting upgrade.",
               peer_id.GetId().c_str(), endpoint_id.c_str());
    return nullptr;
  }

  NEARBY_LOG(INFO,
             "WebRtcBwuHandler successfully connected to remote "
             "peer (%s) while upgrading endpoint %s.",
             peer_id.GetId().c_str(), endpoint_id.c_str());

  // Create a new WebRtcEndpointChannel.
  auto channel = std::make_unique<WebRtcEndpointChannel>(service_id, socket);
  if (channel == nullptr) {
    socket.Close();
    NEARBY_LOG(ERROR,
               "WebRtcBwuHandler failed to create new EndpointChannel for "
               "outgoing socket, aborting upgrade.");
  }

  return channel;
}

void WebrtcBwuHandler::OnEndpointDisconnect(ClientProxy* client,
                                            const std::string& endpoint_id) {}

WebrtcBwuHandler::WebrtcIncomingSocket::WebrtcIncomingSocket(
    const std::string& name, mediums::WebRtcSocketWrapper socket)
    : name_(name), socket_(socket) {}

void WebrtcBwuHandler::WebrtcIncomingSocket::Close() { socket_.Close(); }

std::string WebrtcBwuHandler::WebrtcIncomingSocket::ToString() { return name_; }

}  // namespace connections
}  // namespace nearby
}  // namespace location
