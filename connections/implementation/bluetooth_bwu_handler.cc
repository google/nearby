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

#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/offline_frames.h"

// Manages the Bluetooth-specific methods needed to upgrade an {@link
// EndpointChannel}.

namespace nearby {
namespace connections {

BluetoothBwuHandler::BluetoothBwuHandler(
    Mediums& mediums, IncomingConnectionCallback incoming_connection_callback)
    : BaseBwuHandler(std::move(incoming_connection_callback)),
      mediums_(mediums) {}

// Called by BWU target. Retrieves a new medium info from incoming message,
// and establishes connection over BT using this info.
// Returns a channel ready to exchange data or nullptr on error.
std::unique_ptr<EndpointChannel>
BluetoothBwuHandler::CreateUpgradedEndpointChannel(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id, const UpgradePathInfo& upgrade_path_info) {
  const UpgradePathInfo::BluetoothCredentials& bluetooth_credentials =
      upgrade_path_info.bluetooth_credentials();
  if (!bluetooth_credentials.has_service_name() ||
      !bluetooth_credentials.has_mac_address()) {
    NEARBY_LOG(ERROR, "BluetoothBwuHandler failed to parse UpgradePathInfo.");
    return nullptr;
  }

  const std::string& service_name = bluetooth_credentials.service_name();
  const std::string& mac_address = bluetooth_credentials.mac_address();

  NEARBY_LOGS(VERBOSE) << "BluetoothBwuHandler is attempting to connect to "
                          "available Bluetooth device ("
                       << service_name << ", " << mac_address
                       << ") for endpoint " << endpoint_id << " and service ID "
                       << service_id;

  BluetoothDevice device = bluetooth_medium_.GetRemoteDevice(mac_address);
  if (!device.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "BluetoothBwuHandler failed to derive a valid Bluetooth device "
           "from the MAC address ("
        << mac_address << ") for endpoint " << endpoint_id;
    return nullptr;
  }

  BluetoothSocket socket = bluetooth_medium_.Connect(
      device, service_id, client->GetCancellationFlag(endpoint_id));
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "BluetoothBwuHandler failed to connect to the Bluetooth device ("
        << service_name << ", " << mac_address << ") for endpoint "
        << endpoint_id << " and service ID " << service_id;
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE)
      << "BluetoothBwuHandler successfully connected to Bluetooth device ("
      << service_id << ", " << mac_address << ") while upgrading endpoint "
      << endpoint_id;

  auto channel = std::make_unique<BluetoothEndpointChannel>(
      service_id, /*channel_name=*/service_id, socket);
  if (channel == nullptr) {
    NEARBY_LOGS(ERROR)
        << "BluetoothBwuHandler failed to create Bluetooth endpoint "
           "channel to the Bluetooth device ("
        << service_name << ", " << mac_address << ") for endpoint "
        << endpoint_id << " and service ID " << service_id;
    socket.Close();
    return nullptr;
  }

  return channel;
}

ByteArray BluetoothBwuHandler::HandleInitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& upgrade_service_id,
    const std::string& endpoint_id) {
  std::string mac_address = bluetooth_medium_.GetMacAddress();
  if (mac_address.empty()) {
    NEARBY_LOGS(ERROR) << "BluetoothBwuHandler couldn't initiate the "
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
      NEARBY_LOGS(ERROR) << "BluetoothBwuHandler couldn't initiate the "
                            "BLUETOOTH upgrade for endpoint "
                         << endpoint_id
                         << " because it failed to start listening for "
                            "incoming Bluetooth connections.";

      return {};
    }
    NEARBY_LOGS(VERBOSE)
        << "BluetoothBwuHandler successfully started listening for incoming "
           "Bluetooth connections on service_id="
        << upgrade_service_id << " while upgrading endpoint " << endpoint_id;
  }

  return parser::ForBwuBluetoothPathAvailable(upgrade_service_id, mac_address);
}

void BluetoothBwuHandler::HandleRevertInitiatorStateForService(
    const std::string& upgrade_service_id) {
  bluetooth_medium_.StopAcceptingConnections(upgrade_service_id);
  NEARBY_LOG(INFO,
             "BluetoothBwuHandler successfully reverted all Bluetooth state.");
}

// Accept Connection Callback.
// Notifies that the remote party called BluetoothClassic::Connect()
// for this socket.
void BluetoothBwuHandler::OnIncomingBluetoothConnection(
    ClientProxy* client, const std::string& upgrade_service_id,
    BluetoothSocket socket) {
  auto channel = absl::make_unique<BluetoothEndpointChannel>(
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
