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

#ifdef NO_WEBRTC

#include "connections/implementation/webrtc_bwu_handler_stub.h"

#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/mediums/webrtc_peer_id_stub.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/webrtc_endpoint_channel.h"

namespace nearby {
namespace connections {

WebrtcBwuHandler::WebrtcIncomingSocket::WebrtcIncomingSocket(
    const std::string& name, mediums::WebRtcSocketWrapper socket)
    : name_(name), socket_(socket) {}

void WebrtcBwuHandler::WebrtcIncomingSocket::Close() {}

std::string WebrtcBwuHandler::WebrtcIncomingSocket::ToString() { return ""; }

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
  return nullptr;
}

void WebrtcBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {}

// Called by BWU initiator. Set up WebRTC upgraded medium for this endpoint,
// and returns a upgrade path info (PeerId, LocationHint) for remote party to
// perform discovery.
ByteArray WebrtcBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  return {};
}

// Accept Connection Callback.
// Notifies that the remote party called WebRtc::Connect()
// for this socket.
void WebrtcBwuHandler::OnIncomingWebrtcConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    mediums::WebRtcSocketWrapper socket) {}

}  // namespace connections
}  // namespace nearby

#endif
