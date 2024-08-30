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

#include "connections/implementation/wifi_hotspot_bwu_handler.h"

#include <cstdint>
#include <locale>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_hotspot_endpoint_channel.h"
#include "connections/strategy.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

WifiHotspotBwuHandler::WifiHotspotBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

// Called by BWU initiator. Set up WifiHotspot upgraded medium for this
// endpoint, and returns a upgrade path info (SSID, Password, Gateway used as
// IPAddress, Port) for remote party to perform connection.
ByteArray WifiHotspotBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  // Create SoftAP
  if (!wifi_hotspot_medium_.StartWifiHotspot()) {
    NEARBY_LOGS(INFO) << "Failed to start Wifi Hotspot!";
    return {};
  }

  if (!wifi_hotspot_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_hotspot_medium_.StartAcceptingConnections(
            upgrade_service_id,
            absl::bind_front(
                &WifiHotspotBwuHandler::OnIncomingWifiHotspotConnection, this,
                client))) {
      NEARBY_LOGS(ERROR)
          << "WifiHotspotBwuHandler couldn't initiate WifiHotspot upgrade for "
          << "service " << upgrade_service_id << " and endpoint " << endpoint_id
          << " because it failed to start listening for incoming WifiLan "
             "connections.";
      return {};
    }
    NEARBY_LOGS(INFO)
        << "WifiHotspotBwuHandler successfully started listening for incoming "
           "WifiHotspot connections while upgrading endpoint "
        << endpoint_id;
  }

  // Note: Credentials are not generated until Medium StartWifiHotspot() is
  // called and the server socket is created. Be careful moving this codeblock
  // around.
  HotspotCredentials* hotspot_crendential =
      wifi_hotspot_medium_.GetCredentials(upgrade_service_id);
  std::string ssid = hotspot_crendential->GetSSID();
  std::string password = hotspot_crendential->GetPassword();
  std::string gateway = hotspot_crendential->GetGateway();
  std::int32_t port = hotspot_crendential->GetPort();
  std::int32_t frequency = hotspot_crendential->GetFrequency();

  NEARBY_LOGS(INFO) << "Start SoftAP with SSID:" << ssid
                    << ",  Password:" << password << ",  Port:" << port
                    << ",  Gateway:" << gateway << ",  Frequency:" << frequency;

  bool disabling_encryption =
      (client->GetAdvertisingOptions().strategy == Strategy::kP2pPointToPoint);
  return parser::ForBwuWifiHotspotPathAvailable(
      ssid, password, port, frequency, gateway,
      /* supports_disabling_encryption */ disabling_encryption);
}

void WifiHotspotBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  wifi_hotspot_medium_.StopAcceptingConnections(upgrade_service_id);
  wifi_hotspot_medium_.StopWifiHotspot();
  wifi_hotspot_medium_.DisconnectWifiHotspot();

  NEARBY_LOGS(INFO)
      << "WifiHotspotBwuHandler successfully reverted all states for "
      << "upgrade service ID " << upgrade_service_id;
}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WifiHotspot using this info.
std::unique_ptr<EndpointChannel>
WifiHotspotBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_hotspot_credentials()) {
    NEARBY_LOGS(INFO) << "No Hotspot Credential";
    return nullptr;
  }
  const UpgradePathInfo::WifiHotspotCredentials& upgrade_path_info_credentials =
      upgrade_path_info.wifi_hotspot_credentials();

  const std::string& ssid = upgrade_path_info_credentials.ssid();
  const std::string& password = upgrade_path_info_credentials.password();
  const std::string& gateway = upgrade_path_info_credentials.gateway();
  std::int32_t port = upgrade_path_info_credentials.port();
  std::int32_t frequency = upgrade_path_info_credentials.frequency();

  NEARBY_LOGS(INFO) << "Received Hotspot credential SSID: " << ssid
                    << ",  Password:" << password << ",  Port:" << port
                    << ",  Gateway:" << gateway << ",  Frequency:" << frequency;

  if (!wifi_hotspot_medium_.ConnectWifiHotspot(ssid, password, frequency)) {
    NEARBY_LOGS(ERROR) << "Connect to Hotspot failed";
    return nullptr;
  }

  WifiHotspotSocket socket = wifi_hotspot_medium_.Connect(
      service_id, gateway, port, client->GetCancellationFlag(endpoint_id));
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "WifiHotspotBwuHandler failed to connect to the WifiHotspot service("
        << gateway << ":" << port << ") for endpoint " << endpoint_id;
    return nullptr;
  }

  NEARBY_VLOG(1)
      << "WifiHotspotBwuHandler successfully connected to WifiHotspot service ("
      << gateway << ":" << port << ") while upgrading endpoint " << endpoint_id;

  // Create a new WifiHotspotEndpointChannel.
  auto channel = std::make_unique<WifiHotspotEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket);

  return channel;
}

// Accept Connection Callback.
void WifiHotspotBwuHandler::OnIncomingWifiHotspotConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    WifiHotspotSocket socket) {
  auto channel = std::make_unique<WifiHotspotEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket = std::make_unique<WifiHotspotIncomingSocket>(
              upgrade_service_id, socket),
          .channel = std::move(channel),
      });
  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
