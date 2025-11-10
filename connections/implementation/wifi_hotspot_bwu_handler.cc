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

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <cstdint>
#include <cstring>
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
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/implementation/wifi_hotspot_endpoint_channel.h"
#include "connections/strategy.h"
#include "internal/base/masker.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"
#include "internal/platform/wifi_hotspot.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;

std::vector<char> GatewayToAddressBytes(const std::string& gateway) {
  std::vector<char> address_bytes;
  // Gateway address is IPv4 only.
  uint32_t address_int = inet_addr(gateway.c_str());
  if (address_int == INADDR_NONE) {
    LOG(ERROR) << "Invalid gateway address";
    return address_bytes;
  }
  address_bytes.resize(4);
  std::memcpy(address_bytes.data(),
              reinterpret_cast<char*>(&address_int), 4);
  return address_bytes;
}
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
  std::int32_t frequency = hotspot_crendential->GetFrequency();

  LOG(INFO) << "Start SoftAP with SSID:" << ssid
            << ",  Password:" << masker::Mask(password)
            << ",  Frequency:" << frequency;

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid(ssid);
  credentials.set_password(password);
  credentials.set_frequency(frequency);
  const std::vector<ServiceAddress>& address_candidates =
      hotspot_crendential->GetAddressCandidates();
  for (const auto& service_address : address_candidates) {
    // service address must be either 4 bytes (IPv4) or 16 bytes (IPv6).
    if (service_address.address.size() != 4 &&
        service_address.address.size() != 16) {
      LOG(WARNING) << "Invalid service address size: "
                   << service_address.address.size();
      continue;
    }
    auto* service_address_proto = credentials.add_address_candidates();
    service_address_proto->set_ip_address(std::string(
        service_address.address.begin(), service_address.address.end()));
    service_address_proto->set_port(service_address.port);
  }
  const ServiceAddress& last_service_address = address_candidates.back();
  // We assume the last service address is IPv4.
  credentials.set_gateway(WifiUtils::GetHumanReadableIpAddress(
      std::string(last_service_address.address.begin(),
                  last_service_address.address.end())));
  credentials.set_port(last_service_address.port);
  bool disabling_encryption =
      (client->GetAdvertisingOptions().strategy == Strategy::kP2pPointToPoint);
  return parser::ForBwuWifiHotspotPathAvailable(
      std::move(credentials),
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
  hotspot_credentials.SetFrequency(upgrade_path_info_credentials.frequency());
  std::vector<ServiceAddress> service_addresses;
  for (const auto& service_address :
       upgrade_path_info_credentials.address_candidates()) {
    // service address must be either 4 bytes (IPv4) or 16 bytes (IPv6).
    if (service_address.ip_address().size() != 4 &&
        service_address.ip_address().size() != 16) {
      LOG(WARNING) << "Invalid service address size: "
                   << service_address.ip_address().size();
      continue;
    }
    service_addresses.push_back(ServiceAddress{
        .address = {service_address.ip_address().begin(),
                    service_address.ip_address().end()},
        .port = static_cast<uint16_t>(service_address.port()),
    });
  }
  // Add gateway and port to address candidates if address candidates is empty.
  if (service_addresses.empty() &&
      upgrade_path_info_credentials.has_gateway()) {
    std::vector<char> address_bytes =
        GatewayToAddressBytes(upgrade_path_info_credentials.gateway());
    if (!address_bytes.empty()) {
      service_addresses.push_back(ServiceAddress{
          .address = std::move(address_bytes),
          .port = static_cast<uint16_t>(upgrade_path_info_credentials.port())});
    }
  }
  if (service_addresses.empty()) {
    LOG(ERROR) << "No service address found.";
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_HOTSPOT_INVALID_CREDENTIAL)};
  }
  hotspot_credentials.SetAddressCandidates(std::move(service_addresses));

  LOG(INFO) << "Received Hotspot credential SSID: "
            << hotspot_credentials.GetSSID()
            << ",  Password:" << masker::Mask(hotspot_credentials.GetPassword())
            << ",  Frequency:" << hotspot_credentials.GetFrequency();

  if (!wifi_hotspot_medium_.ConnectWifiHotspot(hotspot_credentials)) {
    LOG(ERROR) << "Connect to Hotspot failed";
    return {Error(
        OperationResultCode::CONNECTIVITY_WIFI_HOTSPOT_INVALID_CREDENTIAL)};
  }

  ErrorOr<WifiHotspotSocket> socket_result = wifi_hotspot_medium_.Connect(
      service_id, hotspot_credentials.GetAddressCandidates(),
      client->GetCancellationFlag(endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR) << "WifiHotspotBwuHandler failed to connect to the WifiHotspot "
                  "service for endpoint "
               << endpoint_id;
    return {Error(socket_result.error().operation_result_code().value())};
  }
  VLOG(1)
      << "WifiHotspotBwuHandler successfully connected to WifiHotspot service "
         "while upgrading endpoint "
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
