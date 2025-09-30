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
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/wifi_hotspot_endpoint_channel.h"
#include "connections/strategy.h"
#include "internal/base/masker.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

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
    LOG(INFO) << "Failed to start Wifi Hotspot!";
    return {};
  }

  if (!wifi_hotspot_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_hotspot_medium_.StartAcceptingConnections(
            upgrade_service_id,
            absl::bind_front(
                &WifiHotspotBwuHandler::OnIncomingWifiHotspotConnection, this,
                client))) {
      LOG(ERROR)
          << "WifiHotspotBwuHandler couldn't initiate WifiHotspot upgrade for "
          << "service " << upgrade_service_id << " and endpoint " << endpoint_id
          << " because it failed to start listening for incoming WifiLan "
             "connections.";
      return {};
    }
    LOG(INFO)
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

  LOG(INFO) << "Start SoftAP with SSID:" << ssid
            << ",  Password:" << masker::Mask(password) << ",  Port:" << port
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

  LOG(INFO) << "WifiHotspotBwuHandler successfully reverted all states for "
            << "upgrade service ID " << upgrade_service_id;
}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WifiHotspot using this info.
ErrorOr<std::unique_ptr<EndpointChannel>>
WifiHotspotBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_hotspot_credentials()) {
    LOG(INFO) << "No Hotspot Credential";
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_HOTSPOT_INVALID_CREDENTIAL)};
  }
  const UpgradePathInfo::WifiHotspotCredentials& upgrade_path_info_credentials =
      upgrade_path_info.wifi_hotspot_credentials();

  HotspotCredentials hotspot_credentials;
  hotspot_credentials.SetSSID(upgrade_path_info_credentials.ssid());
  hotspot_credentials.SetPassword(upgrade_path_info_credentials.password());
  hotspot_credentials.SetGateway(upgrade_path_info_credentials.gateway());
  hotspot_credentials.SetPort(upgrade_path_info_credentials.port());
  hotspot_credentials.SetFrequency(upgrade_path_info_credentials.frequency());

  LOG(INFO) << "Received Hotspot credential SSID: "
            << hotspot_credentials.GetSSID()
            << ",  Password:" << masker::Mask(hotspot_credentials.GetPassword())
            << ",  Port:" << hotspot_credentials.GetPort()
            << ",  Gateway:" << hotspot_credentials.GetGateway()
            << ",  Frequency:" << hotspot_credentials.GetFrequency();

  if (!wifi_hotspot_medium_.ConnectWifiHotspot(hotspot_credentials)) {
    LOG(ERROR) << "Connect to Hotspot failed";
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_HOTSPOT_INVALID_CREDENTIAL)};
  }

  ErrorOr<WifiHotspotSocket> socket_result = wifi_hotspot_medium_.Connect(
      service_id, hotspot_credentials.GetGateway(),
      hotspot_credentials.GetPort(), client->GetCancellationFlag(endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR)
        << "WifiHotspotBwuHandler failed to connect to the WifiHotspot service("
        << hotspot_credentials.GetGateway() << ":"
        << hotspot_credentials.GetPort() << ") for endpoint " << endpoint_id;
    return {Error(socket_result.error().operation_result_code().value())};
  }

  VLOG(1)
      << "WifiHotspotBwuHandler successfully connected to WifiHotspot service ("
      << hotspot_credentials.GetGateway() << ":"
      << hotspot_credentials.GetPort() << ") while upgrading endpoint "
      << endpoint_id;

  // Create a new WifiHotspotEndpointChannel.
  auto channel = std::make_unique<WifiHotspotEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket_result.value());

  return {std::move(channel)};
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
