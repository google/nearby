// Copyright 2022 Google LLC
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

#include "connections/implementation/wifi_direct_bwu_handler.h"

#include <locale>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_direct_endpoint_channel.h"
#include "internal/platform/wifi_direct.h"

namespace nearby {
namespace connections {

WifiDirectBwuHandler::WifiDirectBwuHandler(Mediums& mediums,
                                           BwuNotifications notifications)
    : BaseBwuHandler(std::move(notifications)), mediums_(mediums) {}

ByteArray WifiDirectBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  // Create WifiDirect GO
  if (!wifi_direct_medium_.StartWifiDirect()) {
    NEARBY_LOGS(INFO) << "Failed to start Wifi Direct!";
    return {};
  }

  if (!wifi_direct_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_direct_medium_.StartAcceptingConnections(
            upgrade_service_id,
            {
                .accepted_cb = absl::bind_front(
                    &WifiDirectBwuHandler::OnIncomingWifiDirectConnection, this,
                    client),
            })) {
      NEARBY_LOGS(ERROR)
          << "WifiDirectBwuHandler couldn't initiate WifiDirect upgrade for "
          << "service " << upgrade_service_id << " and endpoint " << endpoint_id
          << " because it failed to start listening for incoming WifiLan "
             "connections.";
      return {};
    }
    NEARBY_LOGS(INFO)
        << "WifiDirectBwuHandler successfully started listening for incoming "
           "WifiDirect connections while upgrading endpoint "
        << endpoint_id;
  }

  // Note: Credentials are not generated until Medium StartWifiDirect() is
  // called and the server socket is created. Be careful moving this codeblock
  // around.
  WifiDirectCredentials* wifi_direct_crendential =
      wifi_direct_medium_.GetCredentials(upgrade_service_id);
  std::string ssid = wifi_direct_crendential->GetSSID();
  std::string password = wifi_direct_crendential->GetPassword();
  std::string gateway = wifi_direct_crendential->GetGateway();
  int port = wifi_direct_crendential->GetPort();
  int freq = wifi_direct_crendential->GetFrequency();

  NEARBY_LOGS(INFO) << "Start WifiDirect GO with SSID: " << ssid
                    << ",  Password: " << password << ",  Port: " << port
                    << ",  Gateway: " << gateway << ", Frequency: " << freq;

  bool disabling_encryption =
      (client->GetAdvertisingOptions().strategy == Strategy::kP2pPointToPoint);
  return parser::ForBwuWifiDirectPathAvailable(
      ssid, password, port, freq,
      /* supports_disabling_encryption */ disabling_encryption, gateway);
}

void WifiDirectBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  wifi_direct_medium_.StopAcceptingConnections(upgrade_service_id);
  wifi_direct_medium_.StopWifiDirect();
  wifi_direct_medium_.DisconnectWifiDirect();

  NEARBY_LOGS(INFO)
      << "WifiDirectBwuHandler successfully reverted all states for "
      << "upgrade service ID " << upgrade_service_id;
}

std::unique_ptr<EndpointChannel>
WifiDirectBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_direct_credentials()) {
    NEARBY_LOGS(INFO) << "No WifiDirect Credential";
    return nullptr;
  }
  const UpgradePathInfo::WifiDirectCredentials& upgrade_path_info_credentials =
      upgrade_path_info.wifi_direct_credentials();

  const std::string& ssid = upgrade_path_info_credentials.ssid();
  const std::string& password = upgrade_path_info_credentials.password();
  std::int32_t port = upgrade_path_info_credentials.port();
  const std::string& gateway = upgrade_path_info_credentials.gateway();

  NEARBY_LOGS(INFO) << "Received WifiDirect credential SSID: " << ssid
                    << ",  Password:" << password << ",  Port:" << port
                    << ",  Gateway:" << gateway;

  if (!wifi_direct_medium_.ConnectWifiDirect(ssid, password)) {
    NEARBY_LOGS(ERROR) << "Connect to WifiDiret GO failed";
    return nullptr;
  }

  WifiDirectSocket socket = wifi_direct_medium_.Connect(
      service_id, gateway, port, client->GetCancellationFlag(endpoint_id));
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "WifiDirectBwuHandler failed to connect to the WifiDirect service("
        << port << ") for endpoint " << endpoint_id;
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE)
      << "WifiDirectBwuHandler successfully connected to WifiDirect service ("
      << port << ") while upgrading endpoint " << endpoint_id;

  // Create a new WifiDirectEndpointChannel.
  return std::make_unique<WifiDirectEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket);
}

void WifiDirectBwuHandler::OnIncomingWifiDirectConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    WifiDirectSocket socket) {
  auto channel = std::make_unique<WifiDirectEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket = std::make_unique<WifiDirectIncomingSocket>(
              upgrade_service_id, socket),
          .channel = std::move(channel),
      });
  bwu_notifications_.incoming_connection_cb(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
