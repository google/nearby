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
#include "internal/platform/wifi_lan.h"

namespace nearby {
namespace connections {

namespace {
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
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_lan_socket()) {
    return {
        Error(OperationResultCode::CONNECTIVITY_WIFI_LAN_INVALID_CREDENTIAL)};
  }
  const UpgradePathInfo::WifiLanSocket& upgrade_path_info_socket =
      upgrade_path_info.wifi_lan_socket();
  if (!upgrade_path_info_socket.has_ip_address() ||
      !upgrade_path_info_socket.has_wifi_port()) {
    LOG(ERROR) << "WifiLanBwuHandler failed to parse UpgradePathInfo.";
    return {Error(OperationResultCode::CONNECTIVITY_WIFI_LAN_IP_ADDRESS_ERROR)};
  }

  const std::string& ip_address = upgrade_path_info_socket.ip_address();
  std::int32_t port = upgrade_path_info_socket.wifi_port();

  VLOG(1) << "WifiLanBwuHandler is attempting to connect to "
          << "available WifiLan service (" << ip_address << ":" << port
          << ") for endpoint " << endpoint_id;

  ErrorOr<WifiLanSocket> socket_result = wifi_lan_medium_.Connect(
      service_id, ip_address, port, client->GetCancellationFlag(endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR) << "WifiLanBwuHandler failed to connect to the WifiLan service ("
               << WifiUtils::GetHumanReadableIpAddress(ip_address) << ":"
               << port << ") for endpoint " << endpoint_id;
    return {Error(socket_result.error().operation_result_code().value())};
  }

  VLOG(1) << "WifiLanBwuHandler successfully connected to WifiLan service ("
          << ip_address << ":" << port << ") while upgrading endpoint "
          << endpoint_id;

  // Create a new WifiLanEndpointChannel.
  auto channel = std::make_unique<WifiLanEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket_result.value());
  if (channel == nullptr) {
    LOG(ERROR) << "WifiLanBwuHandler failed to create WifiLan endpoint "
               << "channel to the WifiLan service (" << ip_address << ":"
               << port << ") for endpoint " << endpoint_id;
    socket_result.value().Close();
    return {Error(
        OperationResultCode::NEARBY_LAN_ENDPOINT_CHANNEL_CREATION_FAILURE)};
  }

  return {std::move(channel)};
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

  // Note: Credentials are not populated until StartAcceptingConnections() is
  // called and the server socket is created. Be careful moving this codeblock
  // around.
  auto credential = wifi_lan_medium_.GetCredentials(upgrade_service_id);
  auto ip_address = credential.first;
  auto port = credential.second;
  if (ip_address.empty()) {
    LOG(INFO) << "WifiLanBwuHandler couldn't initiate the wifi_lan upgrade for "
              << "service " << upgrade_service_id << " and endpoint "
              << endpoint_id
              << " because the wifi_lan ip address were unable to be obtained.";
    return {};
  }

  LOG(INFO) << "WifiLanBwuHandler retrieved WIFI_LAN credentials. IP addr: "
            << ip_address[0] << "." << ip_address[1] << "." << ip_address[2]
            << "." << ip_address[3] << ",  Port: " << port;

  return parser::ForBwuWifiLanPathAvailable(ip_address, port);
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
