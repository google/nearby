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
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ABSL headers
#include "absl/functional/any_invocable.h"
#include "absl/strings/match.h"

// Nearby connections headers
#include "absl/synchronization/mutex.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/windows/wifi_hotspot_socket.h"
#include "internal/platform/logging.h"
#include "internal/platform/service_address.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;
using ::winrt::Windows::Networking::HostNameType;
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;
}  // namespace

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
  bool use_address_candidates = NearbyFlags::GetInstance().GetBoolFlag(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableHotspotAddressCandidates);
  int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
      platform::config_package_nearby::nearby_platform_feature::
          kWifiHotspotCheckIpMaxRetries);
  int64_t ip_address_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotCheckIpIntervalMillis);
  if (!use_address_candidates) {
    // Get current IP addresses of the device.
    VLOG(1) << "maximum IP check retries=" << ip_address_max_retries
            << ", IP check interval=" << ip_address_retry_interval_millis
            << "ms";
    std::string hotspot_ipaddr;
    for (int i = 0; i < ip_address_max_retries; i++) {
      hotspot_ipaddr = GetHotspotIpAddress();
      if (hotspot_ipaddr.empty()) {
        LOG(WARNING) << "Failed to find Hotspot's IP addr for the try: "
                     << i + 1 << ". Wait " << ip_address_retry_interval_millis
                     << "ms snd try again";
        Sleep(ip_address_retry_interval_millis);
      } else {
        break;
      }
    }
    if (hotspot_ipaddr.empty()) {
      LOG(WARNING) << "Failed to start accepting connection without IP "
                      "addresses configured on computer.";
      return;
    }

    std::vector<char> hotspot_ipaddr_bytes;
    uint32_t address_int = inet_addr(hotspot_ipaddr.c_str());
    if (address_int != INADDR_NONE) {
      hotspot_ipaddr_bytes.resize(4);
      std::memcpy(hotspot_ipaddr_bytes.data(),
                  reinterpret_cast<char*>(&address_int), 4);
    }
    ServiceAddress service_address = {
        .address = hotspot_ipaddr_bytes,
        .port = static_cast<uint16_t>(GetPort()),
    };
    hotspot_credentials.SetAddressCandidates({service_address});
    return;
  }
  std::vector<ServiceAddress> service_addresses;
  bool has_ipv4_address = false;
  for (int i = 0; i < ip_address_max_retries; i++) {
    for (const auto& net_interface :
        NetworkInfo::GetNetworkInfo().GetInterfaces()) {
      // service_addresses should only have addresses from a single interface.
      service_addresses.clear();
      if (net_interface.type == InterfaceType::kWifiHotspot) {
        LOG(INFO) << "Found Wifi Hotspot interface, index: "
                  << net_interface.index;
        for (const SocketAddress& ipaddress : net_interface.ipv6_addresses) {
          // IPv6 link-local addresses are allowed and preferred since it skips
          // the DHCP wait time.
          service_addresses.push_back(ipaddress.ToServiceAddress(GetPort()));
        }
        for (const SocketAddress& ipaddress : net_interface.ipv4_addresses) {
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

std::string WifiHotspotServerSocket::GetHotspotIpAddress() const {
  try {
    auto host_names = NetworkInformation::GetHostNames();
    std::vector<std::string> ip_candidates;
    for (auto host_name : host_names) {
      if (host_name.IPInformation() != nullptr &&
          host_name.IPInformation().NetworkAdapter() != nullptr &&
          host_name.Type() == HostNameType::Ipv4) {
        std::string ipv4_s = winrt::to_string(host_name.ToString());
        if (absl::EndsWith(ipv4_s, ".1")) {
          ip_candidates.push_back(ipv4_s);
        }
      }
    }
    if (ip_candidates.empty()) {
      return "";
    }
    // Windows always creates Hotspot at address "192.168.137.1".
    for (auto &ip_candidate : ip_candidates) {
      if (ip_candidate == "192.168.137.1") {
        LOG(INFO) << "Found Hotspot IP: " << ip_candidate;
        return ip_candidate;
      }
    }
    LOG(INFO) << "Found Hotspot IP: " << ip_candidates.front();
    return ip_candidates.front();
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {};
  } catch (const winrt::hresult_error &error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return "";
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return "";
  }
}

}  // namespace nearby::windows
