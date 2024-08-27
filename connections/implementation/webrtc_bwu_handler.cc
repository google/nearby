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

#include "absl/strings/str_cat.h"
#ifndef NO_WEBRTC

#include "connections/implementation/webrtc_bwu_handler.h"

#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/mediums/webrtc_peer_id.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/webrtc_endpoint_channel.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
using ::location::nearby::connections::LocationHint;
using ::location::nearby::connections::LocationStandard;

WebrtcBwuHandler::WebrtcIncomingSocket::WebrtcIncomingSocket(
    const std::string& name, mediums::WebRtcSocketWrapper socket)
    : name_(name), socket_(socket) {}

void WebrtcBwuHandler::WebrtcIncomingSocket::Close() { socket_.Close(); }

std::string WebrtcBwuHandler::WebrtcIncomingSocket::ToString() { return name_; }

WebrtcBwuHandler::WebrtcBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

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
  NEARBY_LOGS(INFO)
      << "WebRtcBwuHandler is attempting to connect to remote peer "
      << peer_id.GetId() << ", location hint "
      << absl::StrCat(location_hint.location());

  mediums::WebRtcSocketWrapper socket =
      webrtc_.Connect(service_id, peer_id, location_hint,
                      client->GetCancellationFlag(endpoint_id));
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR) << "WebRtcBwuHandler failed to connect to remote peer ("
                       << peer_id.GetId() << ") on endpoint " << endpoint_id
                       << ", aborting upgrade.";
    return nullptr;
  }

  NEARBY_LOGS(INFO) << "WebRtcBwuHandler successfully connected to remote "
                       "peer ("
                    << peer_id.GetId() << ") while upgrading endpoint "
                    << endpoint_id;

  // Create a new WebRtcEndpointChannel.
  auto channel = std::make_unique<WebRtcEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket);
  if (channel == nullptr) {
    socket.Close();
    NEARBY_LOGS(ERROR)
        << "WebRtcBwuHandler failed to create new EndpointChannel for "
           "outgoing socket, aborting upgrade.";
  }

  return channel;
}

void WebrtcBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  webrtc_.StopAcceptingConnections(upgrade_service_id);
  NEARBY_LOGS(INFO)
      << "WebrtcBwuHandler successfully reverted state for service "
      << upgrade_service_id;
}

// Called by BWU initiator. Set up WebRTC upgraded medium for this endpoint,
// and returns a upgrade path info (PeerId, LocationHint) for remote party to
// perform discovery.
ByteArray WebrtcBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  LocationHint location_hint =
      Utils::BuildLocationHint(webrtc_.GetDefaultCountryCode());

  mediums::WebrtcPeerId self_id{mediums::WebrtcPeerId::FromRandom()};
  if (!webrtc_.IsAcceptingConnections(upgrade_service_id)) {
    if (!webrtc_.StartAcceptingConnections(
            upgrade_service_id, self_id, location_hint,
            absl::bind_front(&WebrtcBwuHandler::OnIncomingWebrtcConnection,
                             this, client))) {
      NEARBY_LOGS(ERROR) << "WebRtcBwuHandler couldn't initiate the WEB_RTC "
                            "upgrade for endpoint "
                         << endpoint_id
                         << " because it failed to start listening for "
                            "incoming WebRTC connections.";
      return {};
    }
    NEARBY_LOGS(INFO) << "WebRtcBwuHandler successfully started listening for "
                         "incoming WebRTC connections while upgrading endpoint "
                      << endpoint_id;
  }

  return parser::ForBwuWebrtcPathAvailable(self_id.GetId(), location_hint);
}

// Accept Connection Callback.
// Notifies that the remote party called WebRtc::Connect()
// for this socket.
void WebrtcBwuHandler::OnIncomingWebrtcConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    mediums::WebRtcSocketWrapper socket) {
  auto channel = std::make_unique<WebRtcEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  auto webrtc_socket =
      std::make_unique<WebrtcIncomingSocket>(upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{std::move(webrtc_socket),
                                   std::move(channel)});

  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby

#endif
