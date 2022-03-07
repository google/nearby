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

#include "connections/implementation/wifi_hotspot_bwu_handler.h"

#include <locale>
#include <string>

#include "absl/functional/bind_front.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_hotspot_endpoint_channel.h"
#include "internal/platform/wifi_hotspot.h"

namespace location {
namespace nearby {
namespace connections {

WifiHotspotBwuHandler::WifiHotspotBwuHandler(
    Mediums& mediums, EndpointChannelManager& channel_manager,
    BwuNotifications notifications)
    : BaseBwuHandler(channel_manager, std::move(notifications)),
      mediums_(mediums) {}

// Called by BWU initiator. Set up WifiHotspot upgraded medium for this
// endpoint, and returns a upgrade path info (SSID, Password, Gateway used
// IPAddress) for remote party to perform connection.
ByteArray WifiHotspotBwuHandler::InitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id) {
  // Create SoftAP
  if (!wifi_hotspot_medium_.StartWifiHotspot()) {
    NEARBY_LOGS(INFO) << "Failed to start Wifi Hotspot:";
    return {};
  }

  // Use wrapped service ID to avoid have the same ID with the one for
  // startAdvertising. Otherwise, the listening request would be ignored because
  // the medium already start accepting the connection because the client not
  // stop the advertising yet.
  std::string upgrade_service_id = Utils::WrapUpgradeServiceId(service_id);

  if (!wifi_hotspot_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_hotspot_medium_.StartAcceptingConnections(
            upgrade_service_id,
            {
                .accepted_cb = absl::bind_front(
                    &WifiHotspotBwuHandler::OnIncomingWifiHotspotConnection,
                    this, client, service_id),
            })) {
      NEARBY_LOGS(ERROR) << "WifiHotspotBwuHandler couldn't initiate the "
                            "WifiHotspot upgrade for endpoint"
                         << endpoint_id
                         << " because it failed to start listening for "
                            "incoming WifiHotspot connections.";
      return {};
    }
    NEARBY_LOGS(INFO)
        << "WifiHotspotBwuHandler successfully started listening for incoming "
           "WifiHotspot connections while upgrading endpoint "
        << endpoint_id;
  }

  // cache service ID to revert
  active_service_ids_.insert(upgrade_service_id);

  HotspotCredentials* hotspot_crendential =
      wifi_hotspot_medium_.GetCredentials(upgrade_service_id);
  std::string ssid = hotspot_crendential->GetSSID();
  std::string password = hotspot_crendential->GetPassword();
  std::string gateway = hotspot_crendential->GetGateway();
  std::int32_t port = hotspot_crendential->GetPort();

  NEARBY_LOGS(INFO) << "Start SoftAP with SSID:" << ssid
                    << ",  Password:" << password << ",  Port:" << port
                    << ",  Gateway:" << gateway;

  return parser::ForBwuWifiHotspotPathAvailable(ssid, password, port, gateway,
                                                false);
}

void WifiHotspotBwuHandler::Revert() {
  for (const std::string& service_id : active_service_ids_) {
    wifi_hotspot_medium_.StopAcceptingConnections(service_id);
  }
  active_service_ids_.clear();
  wifi_hotspot_medium_.StopWifiHotspot();
  wifi_hotspot_medium_.DisconnectWifiHotspot();

  NEARBY_LOG(INFO, "WifiHotspotBwuHandler successfully reverted all states.");
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
  if (!upgrade_path_info_credentials.has_ssid() ||
      !upgrade_path_info_credentials.has_password()) {
    NEARBY_LOG(ERROR, "WifiHotspotBwuHandler failed to parse UpgradePathInfo.");
    return nullptr;
  }
  const std::string& ssid = upgrade_path_info_credentials.ssid();
  const std::string& password = upgrade_path_info_credentials.password();
  const std::string& gateway = upgrade_path_info_credentials.gateway();
  std::int32_t port = upgrade_path_info_credentials.port();

  NEARBY_LOGS(INFO) << "Received Hotspot credential SSID: " << ssid
                    << ",  Password:" << password << ",  Port:" << port
                    << ",  Gateway:" << gateway;

  HotspotCredentials* hotspot_crendential =
      wifi_hotspot_medium_.GetCredentials(service_id);
  hotspot_crendential->SetSSID(ssid);
  hotspot_crendential->SetPassword(password);
  hotspot_crendential->SetGateway(gateway);
  hotspot_crendential->SetPort(port);

  if (!wifi_hotspot_medium_.ConnectWifiHotspot()) {
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

  NEARBY_LOGS(VERBOSE)
      << "WifiHotspotBwuHandler successfully connected to WifiHotspot service ("
      << gateway << ":" << port << ") while upgrading endpoint " << endpoint_id;

  // Create a new WifiHotspotEndpointChannel.
  auto channel =
      absl::make_unique<WifiHotspotEndpointChannel>(service_id, socket);
  if (channel == nullptr) {
    NEARBY_LOGS(ERROR)
        << "WifiHotspotBwuHandler failed to create WifiHotspot endpoint "
           "channel to the WifiHotspot service ("
        << gateway << ":" << port << ") for endpoint " << endpoint_id;
    socket.Close();
    return nullptr;
  }

  return channel;
}

// Accept Connection Callback.
void WifiHotspotBwuHandler::OnIncomingWifiHotspotConnection(
    ClientProxy* client, const std::string& service_id,
    WifiHotspotSocket socket) {
  auto channel =
      absl::make_unique<WifiHotspotEndpointChannel>(service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket =
              absl::make_unique<WifiHotspotIncomingSocket>(service_id, socket),
          .channel = std::move(channel),
      });
  bwu_notifications_.incoming_connection_cb(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
