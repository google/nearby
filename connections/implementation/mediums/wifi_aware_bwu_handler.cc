// Copyright 2026 Google LLC
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

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/wifi_aware.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;

// The Wi-Fi Aware service name sent to the remote device in the upgrade path
// info.
// TODO: edwinwu - Determine the long-term strategy for declaring and handling
// Wi-Fi Aware service IDs for all Nearby services beyond just Quick Share.
constexpr std::string_view kWifiAwareR4ServiceName = "_qs-aware._tcp";
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
  if (!upgrade_path_info.has_wifi_aware_r4_credentials()) {
    return {
        Error(OperationResultCode::CONNECTIVITY_WIFI_AWARE_INVALID_CREDENTIAL)};
  }
  const BandwidthUpgradeNegotiationFrame::UpgradePathInfo::
      WifiAwareR4Credentials& credentials =
          upgrade_path_info.wifi_aware_r4_credentials();
  // Must happen before StartSubscribing(): that is where a platform decides
  // whether it can reach this peer with the pairings it already has, or has to
  // ask the user to pair first. Empty when the remote device sends no
  // identifier, which platforms treat as "unknown peer".
  wifi_aware_medium_.SetExpectedPeerId(credentials.advertised_name());
  wifi_aware_medium_.StartSubscribing();

  std::string target_service_id = credentials.service_id();
  std::string service_info_bytes;
  std::string passphrase;
  int port = 0;
  if (credentials.has_service_info()) {
    service_info_bytes = credentials.service_info();
  }
  if (credentials.has_pmk()) {
    passphrase = credentials.pmk();
  }
  if (credentials.has_port() && credentials.port() > 0) {
    port = credentials.port();
  }
  LOG(INFO) << "WifiAwareBwuHandler is attempting to connect to available "
               "WifiAware service ("
            << target_service_id << ") for port " << port << " for endpoint "
            << endpoint_id;

  ErrorOr<std::unique_ptr<EndpointChannel>> channel_result =
      wifi_aware_medium_.Connect(
          service_id, target_service_id, ByteArray(service_info_bytes),
          passphrase, port, client->GetCancellationFlag(endpoint_id).get());
  if (channel_result.has_error()) {
    LOG(ERROR) << "WifiAwareBwuHandler failed to connect to the WifiAware "
                  "service ("
               << target_service_id << ") for endpoint " << endpoint_id
               << ", has_error=" << channel_result.has_error();
    wifi_aware_medium_.StopSubscribing();
    return {Error(channel_result.error().operation_result_code().value_or(
        OperationResultCode::DETAIL_UNKNOWN))};
  }
  LOG(INFO)
      << "WifiAwareBwuHandler successfully connected to WifiAware service ("
      << target_service_id << ") while upgrading endpoint " << endpoint_id;

  wifi_aware_medium_.StopSubscribing();
  return channel_result;
}

// Called by BWU initiator. Set up WifiAware upgraded medium for this endpoint,
// and returns an upgrade path info (service_id, service_info) for remote party
// to perform discovery.
std::string WifiAwareBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  if (!wifi_aware_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!wifi_aware_medium_.StartAcceptingConnections(
            upgrade_service_id, /*channel_name=*/upgrade_service_id,
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
  return parser::ForBwuWifiAwareR4PathAvailable(
      kWifiAwareR4ServiceName, /*service_info=*/"", /*pmk=*/"",
      /*port=*/0, /*advertised_name=*/"",
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
    std::unique_ptr<EndpointChannel> channel) {
  LOG(INFO) << "WifiAwareBwuHandler::OnIncomingWifiAwareConnection for "
               "upgrade_service_id "
            << upgrade_service_id;
  auto connection =
      std::make_unique<IncomingSocketConnection>(std::move(channel));
  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
