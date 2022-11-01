// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/g3/wifi_direct.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/g3/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

// Code for WifiDirectMedium
WifiDirectMedium::WifiDirectMedium() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiDirectMedium(*this);
}

WifiDirectMedium::~WifiDirectMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiDirectMedium(*this);
}

bool WifiDirectMedium::StartWifiDirect(
    HotspotCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  std::string ssid = absl::StrCat("DIRECT-", Prng().NextUint32());
  wifi_direct_credentials->SetSSID(ssid);
  std::string password = absl::StrFormat("%08x", Prng().NextUint32());
  wifi_direct_credentials->SetPassword(password);

  NEARBY_LOGS(INFO) << "G3 StartWifiDirect GO: ssid=" << ssid
                    << ",  password:" << password;

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                              /*is_go=*/true, /*enabled=*/true);

  return true;
}

bool WifiDirectMedium::StopWifiDirect() {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "G3 StopWifiDirect GO";

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                              /*is_go=*/true,
                                              /*enabled=*/false);
  return true;
}

bool WifiDirectMedium::ConnectWifiDirect(
    HotspotCredentials* wifi_direct_credentials) {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "G3 ConnectWifiDirect : ssid="
                    << wifi_direct_credentials->GetSSID()
                    << ",  password:" << wifi_direct_credentials->GetPassword();

  auto& env = MediumEnvironment::Instance();
  auto* remote_medium = static_cast<WifiDirectMedium*>(
      env.GetWifiDirectMedium(wifi_direct_credentials->GetSSID(), {}));
  if (!remote_medium) {
    env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                                /*is_go=*/false,
                                                /*enabled=*/false);
    return false;
  }

  env.UpdateWifiDirectMediumForStartOrConnect(*this, wifi_direct_credentials,
                                              /*is_go=*/false,
                                              /*enabled=*/true);
  return true;
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "G3 DisconnectWifiDirect";

  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiDirectMediumForStartOrConnect(*this, /*credentials*/ nullptr,
                                              /*is_go=*/false,
                                              /*enabled=*/false);
  return true;
}

std::unique_ptr<api::WifiHotspotSocket> WifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  std::string socket_name = WifiHotspotServerSocket::GetName(ip_address, port);
  NEARBY_LOGS(INFO) << "G3 WifiDirect ConnectToService [self]: medium=" << this
                    << ", ip address + port=" << socket_name;
  // First, find an instance of remote medium, that exposed this service.
  auto& env = MediumEnvironment::Instance();
  auto* remote_medium =
      static_cast<WifiDirectMedium*>(env.GetWifiDirectMedium({}, ip_address));
  if (remote_medium == nullptr) {
    return nullptr;
  }

  WifiHotspotServerSocket* server_socket = nullptr;
  NEARBY_LOGS(INFO) << "G3 WifiDirect ConnectToService [peer]: medium="
                    << remote_medium
                    << ", remote ip address + port=" << socket_name;
  // Then, find our server socket context in this medium.
  {
    absl::MutexLock medium_lock(&remote_medium->mutex_);
    auto item = remote_medium->server_sockets_.find(socket_name);
    server_socket = item != server_sockets_.end() ? item->second : nullptr;
    if (server_socket == nullptr) {
      NEARBY_LOGS(ERROR) << "G3 WifiDirect Failed to find WifiDirect Server "
                            "socket: socket_name="
                         << socket_name;
      return nullptr;
    }
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR)
        << "G3 WifiDirect Connect: Has been cancelled: socket_name="
        << socket_name;
    return nullptr;
  }

  auto socket = std::make_unique<WifiHotspotSocket>();
  // Finally, Request to connect to this socket.

  server_socket->Connect(*socket);
  NEARBY_LOGS(INFO) << "G3 WifiHotspot GC ConnectToService: connected: socket="
                    << socket.get();
  return socket;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiDirectMedium::ListenForService(int port) {
  auto& env = MediumEnvironment::Instance();
  auto server_socket = std::make_unique<WifiHotspotServerSocket>();

  std::string dot_decimal_ip;
  std::string ip_address = env.GetFakeIPAddress();

  for (auto byte : ip_address) {
    absl::StrAppend(&dot_decimal_ip, absl::StrFormat("%d", byte), ".");
  }
  dot_decimal_ip.pop_back();

  server_socket->SetIPAddress(dot_decimal_ip);
  server_socket->SetPort(port == 0 ? env.GetFakePort() : port);
  std::string socket_name = WifiHotspotServerSocket::GetName(
      server_socket->GetIPAddress(), server_socket->GetPort());
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(socket_name);
  });
  NEARBY_LOGS(INFO) << "G3 WifiDirect GO Adding server socket: medium=" << this
                    << ", socket_name=" << socket_name;
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
