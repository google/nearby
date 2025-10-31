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
#include "internal/platform/exception.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/windows/wifi_hotspot_socket.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;
using ::winrt::Windows::Networking::HostNameType;
using ::winrt::Windows::Networking::Sockets::SocketQualityOfService;
}  // namespace

WifiHotspotServerSocket::WifiHotspotServerSocket(int port) : port_(port) {}

WifiHotspotServerSocket::~WifiHotspotServerSocket() { Close(); }

int WifiHotspotServerSocket::GetPort() const {
  return server_socket_.GetPort();
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotServerSocket::Accept() {
  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  return std::make_unique<WifiHotspotSocket>(std::move(client_socket));
}

void WifiHotspotServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  close_notifier_ = std::move(notifier);
}

Exception WifiHotspotServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }

  server_socket_.Close();
  closed_ = true;

  if (close_notifier_ != nullptr) {
    close_notifier_();
  }

  LOG(INFO) << __func__ << ": Close completed succesfully.";
  return {Exception::kSuccess};
}

void WifiHotspotServerSocket::PopulateHotspotCredentials(
    HotspotCredentials& hotspot_credentials) {
  hotspot_credentials.SetGateway(hotspot_ipaddr_);
  hotspot_credentials.SetPort(port_);
}

bool WifiHotspotServerSocket::Listen(bool dual_stack) {
  // Get current IP addresses of the device.
  int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
      platform::config_package_nearby::nearby_platform_feature::
          kWifiHotspotCheckIpMaxRetries);
  int64_t ip_address_retry_interval_millis =
      NearbyFlags::GetInstance().GetInt64Flag(
          platform::config_package_nearby::nearby_platform_feature::
              kWifiHotspotCheckIpIntervalMillis);
  VLOG(1) << "maximum IP check retries=" << ip_address_max_retries
          << ", IP check interval=" << ip_address_retry_interval_millis << "ms";
  for (int i = 0; i < ip_address_max_retries; i++) {
    hotspot_ipaddr_ = GetHotspotIpAddress();
    if (hotspot_ipaddr_.empty()) {
      LOG(WARNING) << "Failed to find Hotspot's IP addr for the try: " << i + 1
                   << ". Wait " << ip_address_retry_interval_millis
                   << "ms snd try again";
      Sleep(ip_address_retry_interval_millis);
    } else {
      break;
    }
  }
  if (hotspot_ipaddr_.empty()) {
    LOG(WARNING) << "Failed to start accepting connection without IP "
                    "addresses configured on computer.";
    return false;
  }

  SocketAddress address(dual_stack);
  if (!SocketAddress::FromString(address, hotspot_ipaddr_, port_)) {
    LOG(ERROR) << "Failed to parse hotspot IP address: " << hotspot_ipaddr_
               << " and port: " << port_;
    return false;
  }
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket.";
    return false;
  }

  return true;
}

std::string WifiHotspotServerSocket::GetHotspotIpAddress() const {
  try {
    int64_t ip_address_max_retries = NearbyFlags::GetInstance().GetInt64Flag(
        platform::config_package_nearby::nearby_platform_feature::
            kWifiHotspotCheckIpMaxRetries);

    for (int i = 0; i < ip_address_max_retries; i++) {
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
        continue;
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
    }
    return {};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {};
  } catch (const winrt::hresult_error &error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return {};
  }
}

}  // namespace nearby::windows
