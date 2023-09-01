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
#include <cstring>
#include <memory>
#include <random>

#include <systemd/sd-id128.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/wifi_hotspot.h"
#include "internal/platform/implementation/linux/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/linux/wifi_hotspot_socket.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
std::unique_ptr<api::WifiHotspotSocket>
NetworkManagerWifiHotspotMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag *cancellation_flag) {
  if (!WifiHotspotActive()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Cannot connect to service without an active WiFi hotspot";
    return nullptr;
  }

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error opening socket: " << std::strerror(errno);
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE) << __func__ << ": Connecting to " << ip_address << ":"
                       << port;
  struct sockaddr_in addr {};
  addr.sin_addr.s_addr = inet_addr(std::string(ip_address).c_str());
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  auto ret =
      connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error connecting to socket: "
                       << std::strerror(errno);
    return nullptr;
  }

  return std::make_unique<WifiHotspotSocket>(sock);
}

std::unique_ptr<api::WifiHotspotServerSocket>
NetworkManagerWifiHotspotMedium::ListenForService(int port) {
  if (!WifiHotspotActive()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Cannot connect to service without an active WiFi hotspot";
    return nullptr;
  }

  auto active_connection = wireless_device_->GetActiveConnection();
  if (active_connection == nullptr) {
    return nullptr;
  }

  auto ip4addresses = active_connection->GetIP4Addresses();
  if (ip4addresses.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << "Could not find any IPv4 addresses for active connection "
        << active_connection->getObjectPath();
    return nullptr;
  }

  auto sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error opening socket: " << std::strerror(errno);
    return nullptr;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(ip4addresses[0].c_str());
  addr.sin_port = htons(port);

  auto ret =
      bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error binding to socket: " << std::strerror(errno);
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE) << __func__ << ": Listening for services on "
                       << ip4addresses[0] << ":" << port << " on device "
                       << wireless_device_->getObjectPath();

  ret = listen(sock, 0);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error listening on socket: "
                       << std::strerror(errno);
    return nullptr;
  }

  return std::make_unique<NetworkManagerWifiHotspotServerSocket>(
      sock, system_bus_, active_connection->getObjectPath(), network_manager_);
}

bool NetworkManagerWifiHotspotMedium::StartWifiHotspot(
    HotspotCredentials *hotspot_credentials) {
  if (WifiHotspotActive()) {
    NEARBY_LOGS(ERROR) << __func__ << ": " << wireless_device_->getObjectPath()
                       << ": cannot start WiFi hotspot, a hotspot is already "
                          "active on this device";
    return false;
  }

  sd_id128_t id;
  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }

  char id_cstr[SD_ID128_UUID_STRING_MAX];
  sd_id128_to_string(id, id_cstr);

  std::string ssid = absl::StrCat("DIRECT-", id_cstr);
  ssid.resize(32);
  hotspot_credentials->SetSSID(ssid);

  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }

  sd_id128_to_string(id, id_cstr);
  std::string password = std::string(id_cstr, 15);
  hotspot_credentials->SetPassword(password);

  if (auto ret = sd_id128_randomize(&id); ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": error generating a 128-bit ID: "
                       << std::strerror(ret);
    return false;
  }
  sd_id128_to_uuid_string(id, id_cstr);

  std::vector<uint8_t> ssid_bytes(ssid.begin(), ssid.end());
  std::map<std::string, std::map<std::string, sdbus::Variant>>
      connection_settings{
          {
              "connection",
              std::map<std::string, sdbus::Variant>{
                  {"uuid", std::string(id_cstr)},
                  {"id", "Google Nearby Hotspot"},
                  {"type", "802-11-wireless"},
                  {"zone", "Public"}},
          },
          {"802-11-wireless",
           std::map<std::string, sdbus::Variant>{
               {"assigned-mac-address", "random"},
               {"ap-isolation",
                static_cast<std::int32_t>(0)},  // NM_TERNARY_FALSE
               {"mode", "ap"},
               {"ssid", ssid_bytes},
               {"security", "802-11-wireless-security"}}},
          {"802-11-wireless-security",
           std::map<std::string, sdbus::Variant>{
               {"group", std::vector<std::string>{"ccmp"}},
               {"key-mgmt", "wpa-psk"},
               {
                   "pairwise",
                   std::vector<std::string>{"ccmp"},
               },
               {"proto", std::vector<std::string>{"rsn"}},
               {"psk", password}}},
          {"ipv4", std::map<std::string, sdbus::Variant>{{"method", "shared"}}},
          {"ipv6", std::map<std::string, sdbus::Variant>{
                       {"addr-gen-mode", static_cast<std::int32_t>(1)},
                       {"method", "shared"},
                   }}};
  std::unique_ptr<NetworkManagerActiveConnection> active_conn;
  try {
    auto [path, active_path, result] =
        network_manager_->AddAndActivateConnection2(
            connection_settings, wireless_device_->getObjectPath(), "/",
            {{"persist", "volatile"}, {"bind-activation", "dbus-client"}});
    active_conn = std::make_unique<NetworkManagerActiveConnection>(system_bus_,
                                                                   active_path);
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "AddAndActivateConnection2",
                               e);
    return false;
  }

  auto [reason, timeout] = active_conn->WaitForConnection();
  if (timeout) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": "
        << ": timed out while waiting for connection "
        << active_conn->getObjectPath()
        << " to be activated, last NMActiveConnectionStateReason: "
        << reason.value();
    DisconnectWifiHotspot();
    return false;
  }

  NEARBY_LOGS(INFO) << __func__ << ": Started a WiFi hotspot on device "
                    << wireless_device_->getObjectPath() << " at "
                    << active_conn->getObjectPath();
  return true;
}

bool NetworkManagerWifiHotspotMedium::StopWifiHotspot() {
  if (!WifiHotspotActive()) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": " << wireless_device_->getObjectPath()
        << ": Cannot stop WiFi hotspot as a WiFi hotspot is not active";
  }

  // Get the active connection object for the hotspot AP.
  sdbus::ObjectPath active_ap_path;

  try {
    active_ap_path = wireless_device_->ActiveAccessPoint();
    if (active_ap_path.empty()) {
      NEARBY_LOGS(ERROR) << __func__ << ": No active access points on "
                         << wireless_device_->getObjectPath();
      return false;
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "ActiveAccessPoint", e);
  }

  auto object_manager = NetworkManagerObjectManager(system_bus_);
  auto active_connection = wireless_device_->GetActiveConnection();
  if (active_connection == nullptr) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Could not find an active connection using the access point "
        << active_ap_path;
    return false;
  }

  NEARBY_LOGS(INFO) << __func__ << ": " << wireless_device_->getObjectPath()
                    << ": Deactivating active connection "
                    << active_connection->getObjectPath();

  try {
    network_manager_->DeactivateConnection(active_connection->getObjectPath());
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(network_manager_, "DeactivateConnection", e);
    return false;
  }

  return true;
}

bool NetworkManagerWifiHotspotMedium::ConnectWifiHotspot(
    HotspotCredentials *hotspot_credentials) {
  if (hotspot_credentials == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": hotspot_credentials cannot be null";
    return false;
  }

  auto ssid = hotspot_credentials->GetSSID();
  auto password = hotspot_credentials->GetPassword();

  return wireless_device_->ConnectToNetwork(ssid, password,
                                            api::WifiAuthType::kWpaPsk) ==
         api::WifiConnectionStatus::kConnected;
}

bool NetworkManagerWifiHotspotMedium::DisconnectWifiHotspot() {
  if (!ConnectedToWifi()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Not connected to a WiFi hotspot";
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

bool NetworkManagerWifiHotspotMedium::WifiHotspotActive() {
  try {
    auto mode = wireless_device_->Mode();
    return mode == 3;  // NM_802_11_MODE_AP
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "Mode", e);
    return false;
  }
}

bool NetworkManagerWifiHotspotMedium::ConnectedToWifi() {
  try {
    auto mode = wireless_device_->Mode();
    return mode == 2;  // NM_802_11_MODE_INFRA
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(wireless_device_, "Mode", e);
    return false;
  }
}

}  // namespace linux
}  // namespace nearby
