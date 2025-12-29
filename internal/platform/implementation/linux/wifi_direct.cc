// Copyright 2023 Google LLC
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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <memory>

#include "internal/platform/implementation/linux/tcp_server_socket.h"
#include "internal/platform/implementation/linux/wifi_direct.h"
#include "internal/platform/implementation/linux/wifi_direct_server_socket.h"
#include "internal/platform/implementation/linux/wifi_direct_socket.h"
#include "internal/platform/implementation/linux/wifi_hotspot.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace linux {
std::unique_ptr<api::WifiDirectSocket>
NetworkManagerWifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag *cancellation_flag) {
  auto socket = TCPSocket::Connect(std::string(ip_address), port);
  if (!socket.has_value()) return nullptr;

  return std::make_unique<WifiDirectSocket>(std::move(*socket));
}

std::unique_ptr<api::WifiDirectServerSocket>
NetworkManagerWifiDirectMedium::ListenForService(int port) {
  auto active_connection = wireless_device_->GetActiveConnection();
  if (active_connection == nullptr) {
    return nullptr;
  }

  auto ip4addresses = active_connection->GetIP4Addresses();
  if (ip4addresses.empty()) {
    LOG(ERROR)
        << __func__
        << "Could not find any IPv4 addresses for active connection "
        << active_connection->getObjectPath();
    return nullptr;
  }

  auto socket = TCPServerSocket::Listen(std::ref(ip4addresses[0]), port);
  if (!socket.has_value()) return nullptr;

  return std::make_unique<NetworkManagerWifiDirectServerSocket>(
      std::move(*socket), std::move(active_connection), network_manager_);
}

bool NetworkManagerWifiDirectMedium::ConnectWifiDirect(
    WifiDirectCredentials *wifi_direct_credentials) {
  if (wifi_direct_credentials == nullptr) {
    LOG(ERROR) << __func__ << ": hotspot_credentials cannot be null";
    return false;
  }

  auto ssid = wifi_direct_credentials->GetSSID();
  auto password = wifi_direct_credentials->GetPassword();

  return wireless_device_->ConnectToNetwork(ssid, password,
                                            api::WifiAuthType::kWpaPsk) ==
         api::WifiConnectionStatus::kConnected;
}

bool NetworkManagerWifiDirectMedium::DisconnectWifiDirect() {
  if (!ConnectedToWifi()) {
    LOG(ERROR) << __func__ << ": Not connected to a WiFi hotspot";
    return false;
  }

  auto active_connection = wireless_device_->GetActiveConnection();
  if (active_connection == nullptr) {
    return false;
  }

  try {
    network_manager_->DeactivateConnection(active_connection->getObjectPath());
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "DeactivateConnection", e);
    return false;
  }

  return true;
}

bool NetworkManagerWifiDirectMedium::ConnectedToWifi() {
  try {
    auto mode = wireless_device_->Mode();
    return mode == 2;  // NM_802_11_MODE_INFRA
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "Mode", e);
    return false;
  }
}

bool NetworkManagerWifiDirectMedium::StartWifiDirect(
    WifiDirectCredentials *wifi_direct_credentials) {
  // According to the comments in the windows implementation, the wifi direct
  // medium is currently just a regular wifi hotspot.
  auto wireless_device = std::make_unique<NetworkManagerWifiMedium>(
      network_manager_, wireless_device_->getObjectPath());
  auto hotspot = NetworkManagerWifiHotspotMedium(network_manager_,
                                                 std::move(wireless_device));

  HotspotCredentials hotspot_creds;
  if (!hotspot.StartWifiHotspot(&hotspot_creds)) return false;

  wifi_direct_credentials->SetSSID(hotspot_creds.GetSSID());
  wifi_direct_credentials->SetPassword(hotspot_creds.GetPassword());
  return true;
}

bool NetworkManagerWifiDirectMedium::StopWifiDirect() {
  auto wireless_device = std::make_unique<NetworkManagerWifiMedium>(
      network_manager_, wireless_device_->getObjectPath());
  auto hotspot = NetworkManagerWifiHotspotMedium(network_manager_,
                                                 std::move(wireless_device));

  return hotspot.DisconnectWifiHotspot();
}

}  // namespace linux
}  // namespace nearby
