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

#include <exception>
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {
namespace {
constexpr int kWaitingForServerSocketReadyTimeoutSeconds = 60;  // seconds
}  // namespace

WifiDirectServerSocket::~WifiDirectServerSocket() { Close(); }

std::string WifiDirectServerSocket::GetIPAddress() const {
  return wifi_direct_ipaddr_;
}

void WifiDirectServerSocket::SetIPAddress(std::string ip_address) {
  absl::MutexLock lock(mutex_);
  if (ip_address.empty()) {
    return;
  }
  wifi_direct_ipaddr_ = ip_address;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  {
    absl::MutexLock lock(&mutex_);
    if (closed_) return nullptr;
    if (server_socket_accepted_connection_) {
      LOG(INFO) << "Server socket has already accepted a connection. Return.";
      return nullptr;
    }
    LOG(INFO) << "Check if server socket is ready.";
    if (!is_listen_started_) {
      LOG(INFO) <<"Server socket is not started, wait for server socket is "
                   "ready.";
      is_listen_ready_.WaitWithTimeout(
          &mutex_, absl::Seconds(kWaitingForServerSocketReadyTimeoutSeconds));
      if (closed_ || !is_listen_started_) {
        LOG(INFO) << ": Server socket failed to start or was closed.";
        return nullptr;
      }
    }
  }

  LOG(INFO) << "Start to accept connection from WiFiDirect client.";
  auto client_socket = server_socket_.Accept();

  absl::MutexLock lock(&mutex_);
  if (closed_ || client_socket == nullptr) {
    LOG(INFO) << "Accept server socket failed or closed.";
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";
  // This is to indicate that the server socket has accepted a connection.
  server_socket_accepted_connection_ = true;
  return std::make_unique<WifiDirectSocket>(std::move(client_socket));
}

std::string GetWifiDirectGOAddresses() {
  for (int i = 0; i < 3; i++) {
    // Force refresh network info since assignment of the well known
    // static IP address to the hotspot interface does not trigger the IP
    // interface change notification in network_monitor.cc.
    NetworkInfo::GetNetworkInfo().Refresh();
    for (const auto& net_interface :
        NetworkInfo::GetNetworkInfo().GetInterfaces()) {
      if (net_interface.type == InterfaceType::kWifiHotspot) {
        LOG(INFO) << "Found Wifi Hotspot interface, index: "
                  << net_interface.index;
        for (const SocketAddress& ipaddress : net_interface.ipv6_addresses) {
          LOG(INFO) << "Found ipv6 address: " << ipaddress.ToString();
          // IPv6 link-local addresses are allowed and preferred since it skips
          // the DHCP wait time.
        }
        for (const SocketAddress& ipaddress : net_interface.ipv4_addresses) {
          LOG(INFO) << "Found ipv4 address: " << ipaddress.ToString();
          // Skip link-local IPv4 addresses.
          if (ipaddress.IsV4LinkLocal()) {
            LOG(INFO) << "Skip link-local IPv4 address: ";
            continue;
          }

          return ipaddress.ToString();
        }
      }
    }
    LOG(WARNING)
        << "Failed to find Wifi Hotspot interface. Wait 500ms snd try again";
    Sleep(500);
  }
  return "";
}

void WifiDirectServerSocket::PopulateWifiDirectCredentials(
    WifiDirectCredentials& wifi_direct_credentials) {
  std::string wifi_direct_ipaddr = GetWifiDirectGOAddresses();
  wifi_direct_credentials.SetGateway(wifi_direct_ipaddr);
  if (GetPort() != 0) {
  wifi_direct_credentials.SetPort(GetPort());
  } else {
    wifi_direct_credentials.SetPort(FeatureFlags::GetInstance()
                                        .GetFlags()
                                        .wifi_direct_default_port);
  }
}

Exception WifiDirectServerSocket::Close() {
  {
    absl::MutexLock lock(mutex_);
    if (closed_) {
      return {Exception::kSuccess};
    }
    closed_ = true;
    wifi_direct_ipaddr_.clear();
    is_listen_started_ = false;
    server_socket_accepted_connection_ = false;
    is_listen_ready_.SignalAll();
  }

  server_socket_.Close();

  LOG(INFO) << __func__ << ": Close completed succesfully.";
  return {Exception::kSuccess};
}

bool WifiDirectServerSocket::Listen(int port) {
  LOG(INFO) << "Listen wifi_direct on IP:port " << wifi_direct_ipaddr_ << ":"
            << port;
  SocketAddress address;
  if (!SocketAddress::FromString(address, wifi_direct_ipaddr_, port)) {
    LOG(ERROR) << "Failed to parse wifi_direct IP address.";
    return false;
  }
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket.";
    return false;
  }
  LOG(INFO) << "Notify the server socket is started.";
  absl::MutexLock lock(mutex_);
  is_listen_started_ = true;
  is_listen_ready_.SignalAll();

  return true;
}

std::string WifiDirectServerSocket::GetWifiDirectIpAddress() const {
  return wifi_direct_ipaddr_;
}

}  // namespace nearby::windows
