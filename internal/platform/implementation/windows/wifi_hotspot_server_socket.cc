// Copyright 2021 Google LLC
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

#include <windows.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

// Nearby connections headers
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/windows/wifi_hotspot_socket.h"
#include "internal/platform/logging.h"
#include "internal/platform/service_address.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotServerSocket::Accept() {
  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiHotspotSocket>(std::move(client_socket));
}

void WifiHotspotServerSocket::PopulateHotspotCredentials(
    HotspotCredentials& hotspot_credentials) {
  int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
      platform::config_package_nearby::nearby_platform_feature::
          kWifiHotspotCheckIpMaxRetries);
  int64_t ip_address_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotCheckIpIntervalMillis);
  std::vector<ServiceAddress> service_addresses;
  bool has_ipv4_address = false;
  for (int i = 0; i < ip_address_max_retries; i++) {
    // Force refresh network info since assignment of the well known
    // static IP address to the hotspot interface does not trigger the IP
    // interface change notification in network_monitor.cc.
    NetworkInfo::GetNetworkInfo().Refresh();
    for (const auto& net_interface :
        NetworkInfo::GetNetworkInfo().GetInterfaces()) {
      // service_addresses should only have addresses from a single interface.
      service_addresses.clear();
      if (net_interface.type == InterfaceType::kWifiHotspot) {
        LOG(INFO) << "Found Wifi Hotspot interface, index: "
                  << net_interface.index;
        for (const SocketAddress& ipaddress : net_interface.ipv6_addresses) {
          VLOG(1) << "Found ipv6 address: " << ipaddress.ToString();
          // IPv6 link-local addresses are allowed and preferred since it skips
          // the DHCP wait time.
          service_addresses.push_back(ipaddress.ToServiceAddress(GetPort()));
        }
        for (const SocketAddress& ipaddress : net_interface.ipv4_addresses) {
          VLOG(1) << "Found ipv4 address: " << ipaddress.ToString();
          // Skip link-local IPv4 addresses.
          if (ipaddress.IsV4LinkLocal()) {
            continue;
          }
          has_ipv4_address = true;
          service_addresses.push_back(ipaddress.ToServiceAddress(GetPort()));
        }
        // We assume there is only one Wifi Hotspot interface.  So stop looking
        // once we've found a hotspot interface with a non-link-local IPv4
        // address.
        if (has_ipv4_address) {
          break;
        }
      }
    }
    if (has_ipv4_address && !service_addresses.empty()) {
      break;
    }
    LOG(WARNING) << "Failed to find Wifi Hotspot interface. Wait "
                 << ip_address_retry_interval_millis
                 << "ms snd try again";
    Sleep(ip_address_retry_interval_millis);
  }
  LOG(INFO) << "Found " << service_addresses.size() << " hotspot addresses";
  hotspot_credentials.SetAddressCandidates(std::move(service_addresses));
}

bool WifiHotspotServerSocket::Listen(int port) {
  // Allow server socket to listen on all interfaces.
  // Consider sharing the same server socket for WifiLan medium.
  SocketAddress address;
  address.set_port(port);
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket.";
    return false;
  }
  return true;
}

}  // namespace nearby::windows
