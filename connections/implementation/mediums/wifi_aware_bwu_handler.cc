// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/wifi_aware_bwu_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/strings/numbers.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/wifi_aware.h"
#include "connections/implementation/mediums/wifi_aware_endpoint_channel.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_aware.h"
#include "internal/platform/wifi_aware_service_info.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

WifiAwareBwuHandler::WifiAwareBwuHandler(
    WifiAware& wifi_aware_medium,
    IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      wifi_aware_medium_(wifi_aware_medium) {}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over WifiAware using this info.
ErrorOr<std::unique_ptr<EndpointChannel>>
WifiAwareBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id,
    const BandwidthUpgradeNegotiationFrame::UpgradePathInfo&
        upgrade_path_info) {
  if (!upgrade_path_info.has_wifi_aware_credentials()) {
    return {
        Error(OperationResultCode::CONNECTIVITY_WIFI_AWARE_INVALID_CREDENTIAL)};
  }
  wifi_aware_medium_.StartSubscribing();
  const BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiAwareCredentials&
      credentials = upgrade_path_info.wifi_aware_credentials();

  LOG(INFO) << "WifiAwareBwuHandler is attempting to connect to available "
               "WifiAware service ("
            << credentials.service_id() << ") for endpoint " << endpoint_id;

  // We need to construct a WifiAwareServiceInfo from the credentials.
  // WifiAwareMedium::Connect takes WifiAwareServiceInfo.
  WifiAwareServiceInfo service_info;
  int port = 0;
  std::string service_name = credentials.service_id();
  auto colon_pos = service_name.rfind(':');
  if (colon_pos != std::string::npos) {
    std::string port_str = service_name.substr(colon_pos + 1);
    if (absl::SimpleAtoi(port_str, &port)) {
      service_name = service_name.substr(0, colon_pos);
    }
  }

  if (port == 0 && credentials.has_service_info()) {
    const std::string& info = credentials.service_info();
    auto pos = info.find("port=");
    if (pos != std::string::npos) {
      std::string port_str = info.substr(pos + 5);
      auto end_pos = port_str.find_first_of(";\r\n ");
      if (end_pos != std::string::npos) {
        port_str = port_str.substr(0, end_pos);
      }
      if (!absl::SimpleAtoi(port_str, &port)) {
        port = 0;
      }
    } else if (info.size() == 2) {
      uint8_t byte0 = static_cast<uint8_t>(info[0]);
      uint8_t byte1 = static_cast<uint8_t>(info[1]);
      int parsed_port = (byte0 << 8) | byte1;
      if (parsed_port > 0 && parsed_port <= 65535) {
        port = parsed_port;
      }
    }
  }

  service_info.SetServiceName(service_name);
  service_info.SetServiceType(service_name);
  if (port > 0) {
    service_info.SetPort(port);
  }
  if (credentials.has_service_info()) {
    service_info.SetTxtRecord("info", credentials.service_info());
  }

  ErrorOr<WifiAwareSocket> socket_result = wifi_aware_medium_.Connect(
      service_id, service_info, client->GetCancellationFlag(endpoint_id).get());
  if (socket_result.has_error()) {
    LOG(ERROR) << "WifiAwareBwuHandler failed to connect to the WifiAware "
                  "service ("
               << credentials.service_id() << ") for endpoint " << endpoint_id;
    wifi_aware_medium_.StopSubscribing();
    return {Error(socket_result.error().operation_result_code().value_or(
        OperationResultCode::DETAIL_UNKNOWN))};
  }
  LOG(INFO)
      << "WifiAwareBwuHandler successfully connected to WifiAware service ("
      << credentials.service_id() << ") while upgrading endpoint "
      << endpoint_id;

  wifi_aware_medium_.StopSubscribing();

  // Create a new WifiAwareEndpointChannel.
  auto channel = std::make_unique<WifiAwareEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket_result.value());
  return {std::move(channel)};
}

// Called by BWU initiator. Set up WifiAware upgraded medium for this endpoint,
// and returns an upgrade path info (service_id, service_info) for remote party
// to perform discovery.
std::string WifiAwareBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  if (!wifi_aware_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_aware_medium_.StartAcceptingConnections(
            upgrade_service_id,
            absl::bind_front(
                &WifiAwareBwuHandler::OnIncomingWifiAwareConnection, this,
                client))) {
      LOG(ERROR) << "WifiAwareBwuHandler::"
                    "HandleInitializeUpgradedMediumForEndpoint failed";
      return "";
    }
  }
  LOG(INFO) << "WifiAwareBwuHandler successfully initialized upgraded medium "
               "for endpoint "
            << endpoint_id;
  return parser::ForBwuWifiAwarePathAvailable(
      "_qs-aware._tcp", /*service_info=*/"", /*password=*/"",
      /*supports_disabling_encryption=*/false);
}

void WifiAwareBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  wifi_aware_medium_.StopAcceptingConnections(upgrade_service_id);
  LOG(INFO) << "WifiAwareBwuHandler successfully reverted all states for "
            << "upgrade service ID " << upgrade_service_id;
}

// Accept Connection Callback.
void WifiAwareBwuHandler::OnIncomingWifiAwareConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    WifiAwareSocket socket) {
  LOG(INFO) << "WifiAwareBwuHandler::OnIncomingWifiAwareConnection for "
               "upgrade_service_id "
            << upgrade_service_id;
  auto channel = std::make_unique<WifiAwareEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket = std::make_unique<WifiAwareIncomingSocket>(
              upgrade_service_id, std::move(socket)),
          .channel = std::move(channel),
      });
  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
