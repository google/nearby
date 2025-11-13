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

#include "connections/implementation/wifi_lan_bwu_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_lan_endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_lan.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

WifiLanBwuHandler::WifiLanBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WifiLan using this info.
ErrorOr<std::unique_ptr<EndpointChannel>>
WifiLanBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id,
    const BandwidthUpgradeNegotiationFrame::UpgradePathInfo&
        upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_lan_socket()) {
    return {
        Error(OperationResultCode::CONNECTIVITY_WIFI_LAN_INVALID_CREDENTIAL)};
  }
  const BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiLanSocket&
      upgrade_path_info_socket = upgrade_path_info.wifi_lan_socket();
  if ((!upgrade_path_info_socket.has_ip_address() ||
       !upgrade_path_info_socket.has_wifi_port()) &&
      upgrade_path_info_socket.address_candidates_size() == 0) {
    LOG(ERROR) << "WifiLanBwuHandler failed to parse UpgradePathInfo.";
    return {Error(OperationResultCode::CONNECTIVITY_WIFI_LAN_IP_ADDRESS_ERROR)};
  }

  std::vector<ServiceAddress> address_candidates;
  for (const auto& address_candidate :
       upgrade_path_info_socket.address_candidates()) {
    if (address_candidate.has_ip_address() && address_candidate.has_port()) {
      address_candidates.push_back(ServiceAddress{
          .address = std::vector<char>(address_candidate.ip_address().begin(),
                                       address_candidate.ip_address().end()),
          .port = static_cast<uint16_t>(address_candidate.port())});
    }
  }
  // Only use ip_address and wifi_port if address_candidates is empty.
  if (address_candidates.empty()) {
    address_candidates.push_back(ServiceAddress{
        .address =
            std::vector<char>(upgrade_path_info_socket.ip_address().begin(),
                              upgrade_path_info_socket.ip_address().end()),
        .port = static_cast<uint16_t>(upgrade_path_info_socket.wifi_port())});
  }
  Error error;
  for (const auto& address_candidate : address_candidates) {
    std::string ip_address = std::string(address_candidate.address.begin(),
                                         address_candidate.address.end());
    int port = address_candidate.port;
    VLOG(1) << "WifiLanBwuHandler is attempting to connect to available "
               "WifiLan service ("
            << WifiUtils::GetHumanReadableIpAddress(ip_address) << ":" << port
            << ") for endpoint " << endpoint_id;
    ErrorOr<WifiLanSocket> socket_result = wifi_lan_medium_.Connect(
        service_id, ip_address, port, client->GetCancellationFlag(endpoint_id));
    if (socket_result.has_error()) {
      LOG(ERROR)
          << "WifiLanBwuHandler failed to connect to the WifiLan service ("
          << WifiUtils::GetHumanReadableIpAddress(ip_address) << ":" << port
          << ") for endpoint " << endpoint_id;
      error = Error(socket_result.error().operation_result_code().value());
      continue;
    }
    VLOG(1) << "WifiLanBwuHandler successfully connected to WifiLan service ("
            << WifiUtils::GetHumanReadableIpAddress(ip_address) << ":" << port
            << ") while upgrading endpoint " << endpoint_id;

    // Create a new WifiLanEndpointChannel.
    auto channel = std::make_unique<WifiLanEndpointChannel>(
        service_id, /*channel_name=*/service_id, socket_result.value());
    return {std::move(channel)};
  }

  return {error};
}

// Called by BWU initiator. Set up WifiLan upgraded medium for this endpoint,
// and returns a upgrade path info (ip address, port) for remote party to
// perform discovery.
ByteArray WifiLanBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  if (!wifi_lan_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_lan_medium_.StartAcceptingConnections(
            upgrade_service_id,
            absl::bind_front(&WifiLanBwuHandler::OnIncomingWifiLanConnection,
                             this, client))) {
      LOG(ERROR)
          << "WifiLanBwuHandler couldn't initiate the WifiLan upgrade for "
          << "service " << upgrade_service_id << " and endpoint " << endpoint_id
          << " because it failed to start listening for incoming WifiLan "
             "connections.";
      return {};
    }
    LOG(INFO)
        << "WifiLanBwuHandler successfully started listening for incoming "
           "WifiLan connections while upgrading endpoint "
        << endpoint_id;
  }

  // Address candidates are not populated until StartAcceptingConnections() is
  // called and the server socket is created. Be careful moving this codeblock
  // around.
  std::pair<std::vector<std::string>, int> upgrade_candidates =
      wifi_lan_medium_.GetUpgradeAddressCandidates(upgrade_service_id);
  const std::vector<std::string>& ip_addresses = upgrade_candidates.first;
  int port = upgrade_candidates.second;
  if (ip_addresses.empty()) {
    LOG(INFO) << "WifiLanBwuHandler couldn't initiate the wifi_lan upgrade for "
              << "service " << upgrade_service_id << " and endpoint "
              << endpoint_id << " because there are no available ip addresses.";
    return {};
  }
  return parser::ForBwuWifiLanPathAvailable(ip_addresses, port);
}

void WifiLanBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  wifi_lan_medium_.StopAcceptingConnections(upgrade_service_id);
  LOG(INFO) << "WifiLanBwuHandler successfully reverted all states for "
            << "upgrade service ID " << upgrade_service_id;
}

// Accept Connection Callback.
void WifiLanBwuHandler::OnIncomingWifiLanConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    WifiLanSocket socket) {
  auto channel = std::make_unique<WifiLanEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket = std::make_unique<WifiLanIncomingSocket>(upgrade_service_id,
                                                            socket),
          .channel = std::move(channel),
      });
  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
