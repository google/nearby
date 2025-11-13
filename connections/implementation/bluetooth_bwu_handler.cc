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

#include "connections/implementation/bluetooth_bwu_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"

// Manages the Bluetooth-specific methods needed to upgrade an {@link
// EndpointChannel}.

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::proto::connections::OperationResultCode;
}  // namespace

BluetoothBwuHandler::BluetoothBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over BT using this info.
// Returns a channel ready to exchange data or nullptr on error.
ErrorOr<std::unique_ptr<EndpointChannel>>
BluetoothBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id,
    const BandwidthUpgradeNegotiationFrame::UpgradePathInfo&
        upgrade_path_info) {
  const BandwidthUpgradeNegotiationFrame::UpgradePathInfo::BluetoothCredentials&
      bluetooth_credentials = upgrade_path_info.bluetooth_credentials();
  if (!bluetooth_credentials.has_service_name() ||
      !bluetooth_credentials.has_mac_address()) {
    LOG(ERROR) << "BluetoothBwuHandler failed to parse UpgradePathInfo.";
    return {
        Error(OperationResultCode::CONNECTIVITY_BLUETOOTH_INVALID_CREDENTIAL)};
  }

  const std::string& service_name = bluetooth_credentials.service_name();
  MacAddress mac_address;
  MacAddress::FromString(bluetooth_credentials.mac_address(), mac_address);

  VLOG(1) << "BluetoothBwuHandler is attempting to connect to "
             "available Bluetooth device ("
          << service_name << ", " << mac_address.ToString() << ") for endpoint "
          << endpoint_id << " and service ID " << service_id;

  BluetoothDevice device = bluetooth_medium_.GetRemoteDevice(mac_address);
  if (!device.IsValid()) {
    LOG(ERROR)
        << "BluetoothBwuHandler failed to derive a valid Bluetooth device "
           "from the MAC address ("
        << mac_address.ToString() << ") for endpoint " << endpoint_id;
    return {Error(
        OperationResultCode::CONNECTIVITY_BLUETOOTH_DEVICE_OBTAIN_FAILURE)};
  }

  ErrorOr<BluetoothSocket> socket_result = bluetooth_medium_.Connect(
      device, service_id, client->GetCancellationFlag(endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR)
        << "BluetoothBwuHandler failed to connect to the Bluetooth device ("
        << service_name << ", " << mac_address.ToString() << ") for endpoint "
        << endpoint_id << " and service ID " << service_id;
    return {Error(socket_result.error().operation_result_code().value())};
  }

  VLOG(1) << "BluetoothBwuHandler successfully connected to Bluetooth device ("
          << service_id << ", " << mac_address.ToString()
          << ") while upgrading endpoint " << endpoint_id;

  auto channel = std::make_unique<BluetoothEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket_result.value());
  if (channel == nullptr) {
    LOG(ERROR) << "BluetoothBwuHandler failed to create Bluetooth endpoint "
                  "channel to the Bluetooth device ("
               << service_name << ", " << mac_address.ToString()
               << ") for endpoint " << endpoint_id << " and service ID "
               << service_id;
    socket_result.value().Close();
    return {Error(
        OperationResultCode::NEARBY_BT_ENDPOINT_CHANNEL_CREATION_FAILURE)};
  }

  client->SetBluetoothMacAddress(endpoint_id, mac_address);
  return {std::move(channel)};
}

ByteArray BluetoothBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  MacAddress mac_address = bluetooth_medium_.GetAddress();
  if (!mac_address.IsSet()) {
    LOG(ERROR) << "BluetoothBwuHandler couldn't initiate the "
                  "BLUETOOTH upgrade for service ID "
               << upgrade_service_id << " and endpoint " << endpoint_id
               << " because MAC address is empty.";
    return {};
  }

  if (!bluetooth_medium_.IsAcceptingConnections(upgrade_service_id)) {
    if (!bluetooth_medium_.StartAcceptingConnections(
            upgrade_service_id,
            absl::bind_front(
                &BluetoothBwuHandler::OnIncomingBluetoothConnection, this,
                client))) {
      LOG(ERROR) << "BluetoothBwuHandler couldn't initiate the "
                    "BLUETOOTH upgrade for endpoint "
                 << endpoint_id
                 << " because it failed to start listening for "
                    "incoming Bluetooth connections.";

      return {};
    }
    VLOG(1)
        << "BluetoothBwuHandler successfully started listening for incoming "
           "Bluetooth connections on service_id="
        << upgrade_service_id << " while upgrading endpoint " << endpoint_id;
  }

  return parser::ForBwuBluetoothPathAvailable(upgrade_service_id, mac_address);
}

void BluetoothBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  bluetooth_medium_.StopAcceptingConnections(upgrade_service_id);
  LOG(INFO) << "BluetoothBwuHandler successfully reverted all Bluetooth state.";
}

// Accept Connection Callback.
// Notifies that the remote party called BluetoothClassic::Connect()
// for this socket.
void BluetoothBwuHandler::OnIncomingBluetoothConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    BluetoothSocket socket) {
  auto channel = std::make_unique<BluetoothEndpointChannel>(
      upgrade_service_id, /*channel_name=*/upgrade_service_id, socket);
  std::unique_ptr<IncomingSocketConnection> connection{
      new IncomingSocketConnection{
          .socket = std::make_unique<BluetoothIncomingSocket>(
              upgrade_service_id, socket),
          .channel = std::move(channel),
      }};
  NotifyOnIncomingConnection(client, std::move(connection));
}

}  // namespace connections
}  // namespace nearby
