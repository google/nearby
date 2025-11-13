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

#include "connections/implementation/awdl_bwu_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/awdl_endpoint_channel.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/awdl.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/service_id_constants.h"
#include "internal/base/masker.h"
#include "internal/platform/awdl.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;

constexpr absl::Duration kAwdlDiscoveryTimeout = absl::Seconds(5);
constexpr int kServiceNameLength = 8;
constexpr int kPasswordLength = 16;
constexpr absl::string_view kPskIdentity = "AwdlUpgradeMedium";
constexpr absl::string_view kAwdlServiceIdSuffixForServiceType = "_AWDL";
}  // namespace

AwdlBwuHandler::AwdlBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over AWDL using this info.
ErrorOr<std::unique_ptr<EndpointChannel>>
AwdlBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id,
    const BandwidthUpgradeNegotiationFrame::UpgradePathInfo&
        upgrade_path_info) {
  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(service_id);
  if (!upgrade_path_info.has_awdl_credentials()) {
    return {Error(OperationResultCode::CONNECTIVITY_AWDL_INVALID_CREDENTIAL)};
  }

  const BandwidthUpgradeNegotiationFrame::UpgradePathInfo::AwdlCredentials&
      awdl_credentials = upgrade_path_info.awdl_credentials();
  if (!awdl_credentials.has_service_name() ||
      !awdl_credentials.has_service_type() ||
      !awdl_credentials.has_password()) {
    LOG(ERROR) << "Failed to upgrade AWDL due to invalid credentials.";
    return {Error(OperationResultCode::CONNECTIVITY_AWDL_INVALID_CREDENTIAL)};
  }

  std::string service_name = awdl_credentials.service_name();
  std::string service_type = awdl_credentials.service_type();
  std::string password = awdl_credentials.password();

  LOG(INFO) << "Attempting to connect to "
            << "AWDL (service_name:" << service_name
            << ", service_type:" << service_type
            << ", password:" << masker::Mask(password) << ") for endpoint "
            << endpoint_id;

  NsdServiceInfo nsd_service_info{};
  nsd_service_info.SetServiceName(service_name);
  nsd_service_info.SetServiceType(service_type);

  api::PskInfo psk_info = {
      .identity = std::string(kPskIdentity),
      .password = password,
  };

  LOG(INFO) << "Start to discover the AWDL service "
               "(service_name:"
            << service_name << ", service_type:" << service_type
            << ") for endpoint " << endpoint_id;

  CountDownLatch latch(1);

  // Needs to discover the upgrade service before connecting to it.
  if (!awdl_medium_.StartDiscovery(
          upgrade_service_id,
          {
              .service_discovered_cb =
                  [service_name, &latch](const NsdServiceInfo& service_info,
                                         const std::string& service_id) {
                    if (service_info.GetServiceName() == service_name) {
                      LOG(INFO)
                          << "Discovered the "
                          << "AWDL service (service_name:" << service_name
                          << ", service_type:" << service_info.GetServiceType()
                          << ") successfully";
                      latch.CountDown();
                    }
                  },
              .service_lost_cb = [](NsdServiceInfo service_info,
                                    const std::string& service_id) {},
          })) {
    LOG(ERROR) << "Failed to discover the AWDL service (service_name:"
               << service_name << ", service_type:" << service_type
               << ") for endpoint " << endpoint_id;
    return {Error(
        OperationResultCode::NEARBY_AWDL_ENDPOINT_CHANNEL_CREATION_FAILURE)};
  }

  if (!latch.Await(kAwdlDiscoveryTimeout)) {
    LOG(ERROR) << "Failed to discover the AWDL service (service_name:"
               << service_name << ", service_type:" << service_type
               << ") due to timeout.";
    awdl_medium_.StopDiscovery(upgrade_service_id);
    return {Error(
        OperationResultCode::NEARBY_AWDL_ENDPOINT_CHANNEL_CREATION_FAILURE)};
  }

  LOG(INFO) << "Discovered the AWDL service "
               "(service_name:"
            << service_name << ", service_type:" << service_type
            << ") for endpoint " << endpoint_id;

  ErrorOr<AwdlSocket> socket_result =
      awdl_medium_.Connect(upgrade_service_id, nsd_service_info, psk_info,
                           client->GetCancellationFlag(endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR) << "Failed to connect to the AWDL service (service_name:"
               << service_name << ", service_type:" << service_type
               << ") for endpoint " << endpoint_id;
    awdl_medium_.StopDiscovery(upgrade_service_id);
    return {Error(socket_result.error().operation_result_code().value())};
  }

  LOG(INFO) << "Connected to AWDL service (service_name:" << service_name
            << ", service_type:" << service_type
            << ") successfully while upgrading endpoint " << endpoint_id;

  // Create a new AwdlEndpointChannel.
  auto channel = std::make_unique<AwdlEndpointChannel>(
      upgrade_service_id,
      /*channel_name=*/upgrade_service_id, socket_result.value(), &awdl_medium_,
      /*is_outgoing=*/true);
  if (channel == nullptr) {
    LOG(ERROR) << "Failed to create AWDL endpoint "
               << "channel to the AWDL service (service_name:" << service_name
               << ", service_type:" << service_type << ") for endpoint "
               << endpoint_id;
    awdl_medium_.StopDiscovery(upgrade_service_id);
    socket_result.value().Close();
    return {Error(
        OperationResultCode::NEARBY_AWDL_ENDPOINT_CHANNEL_CREATION_FAILURE)};
  }

  return {std::move(channel)};
}

// Called by BWU initiator. Set up AWDL upgraded medium for this endpoint,
// and returns a upgrade path info (service_name, port) for remote party to
// perform discovery.
ByteArray AwdlBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  if (!awdl_medium_.IsAcceptingConnections(upgrade_service_id)) {
    api::PskInfo psk_info = {
        .identity = std::string(kPskIdentity),
        .password = GeneratePassword(),
    };
    if (!awdl_medium_.StartAcceptingConnections(
            upgrade_service_id, psk_info,
            absl::bind_front(&AwdlBwuHandler::OnIncomingAwdlConnection, this,
                             client))) {
      LOG(ERROR) << "Failed to initiate the AWDL upgrade for "
                 << "service " << upgrade_service_id << " and endpoint "
                 << endpoint_id
                 << " because it failed to start listening for incoming AWDL "
                    "connections.";
      return {};
    }

    // Need to advertise the service.
    nsd_service_info_ = NsdServiceInfo();
    nsd_service_info_.SetServiceName(GenerateServiceName());
    nsd_service_info_.SetServiceType(GenerateServiceType(
        absl::StrCat(upgrade_service_id, kAwdlServiceIdSuffixForServiceType)));

    if (!awdl_medium_.StartAdvertising(upgrade_service_id, nsd_service_info_)) {
      LOG(ERROR) << "Failed to initiate the AWDL upgrade for "
                 << "service " << upgrade_service_id << " and endpoint "
                 << endpoint_id << " because it failed to start advertising.";
      awdl_medium_.StopAcceptingConnections(upgrade_service_id);
      return {};
    }

    LOG(INFO) << "Started listening for incoming AWDL connections while "
                 "upgrading endpoint "
              << endpoint_id;
  }

  // Note: Credentials are not populated until StartAcceptingConnections() is
  // called and the server socket is created. Be careful moving this codeblock
  // around.
  Awdl::AwdlCredential credential =
      awdl_medium_.GetCredentials(upgrade_service_id);
  std::string service_name = credential.service_name;
  std::string service_type = credential.service_type;
  std::string password = credential.password;

  LOG(INFO) << "Retrieved AWDL credentials. service_name: " << service_name
            << " and service_type: " << service_type
            << " and password: " << masker::Mask(password);

  // The AWDL upgraded medium is running under TLS, so we don't need to
  // encryption for it again.
  return parser::ForBwuAwdlPathAvailable(
      service_name, service_type, password,
      /*supports_disabling_encryption=*/true);
}

void AwdlBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  awdl_medium_.StopAdvertising(upgrade_service_id);
  awdl_medium_.StopAcceptingConnections(upgrade_service_id);
  LOG(INFO) << "Reverted all states for "
            << "upgrade service ID " << upgrade_service_id << " successfully.";
}

// Accept Connection Callback.
void AwdlBwuHandler::OnIncomingAwdlConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    AwdlSocket socket) {
  LOG(INFO) << "Accepted connection for upgrade service ID "
            << upgrade_service_id;
  auto channel = std::make_unique<AwdlEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket,
      &awdl_medium_, /*is_outgoing=*/false);
  std::unique_ptr<IncomingSocketConnection> connection(
      new IncomingSocketConnection{
          .socket =
              std::make_unique<AwdlIncomingSocket>(upgrade_service_id, socket),
          .channel = std::move(channel),
      });
  NotifyOnIncomingConnection(client, std::move(connection));
}

std::string AwdlBwuHandler::GenerateServiceType(const std::string& service_id) {
  std::string service_id_hash_string;

  const ByteArray service_id_hash = Utils::Sha256Hash(
      service_id, NsdServiceInfo::kTypeFromServiceIdHashLength);
  for (auto byte : std::string(service_id_hash)) {
    absl::StrAppend(&service_id_hash_string, absl::StrFormat("%02X", byte));
  }

  return absl::StrFormat(NsdServiceInfo::kNsdTypeFormat,
                         service_id_hash_string);
}

std::string AwdlBwuHandler::GenerateServiceName() {
  std::string service_name_string;
  ByteArray ramdon_bytes = Utils::GenerateRandomBytes(kServiceNameLength);
  for (auto byte : std::string(ramdon_bytes)) {
    absl::StrAppend(&service_name_string, absl::StrFormat("%02X", byte));
  }
  return service_name_string;
}

std::string AwdlBwuHandler::GeneratePassword() {
  std::string password_string;
  ByteArray ramdon_bytes = Utils::GenerateRandomBytes(kPasswordLength);
  for (auto byte : std::string(ramdon_bytes)) {
    absl::StrAppend(&password_string, absl::StrFormat("%02X", byte));
  }
  return password_string;
}

}  // namespace connections
}  // namespace nearby
