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

#include <netinet/in.h>
#include <sys/socket.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/wifi_direct_server_socket.h"
#include "internal/platform/implementation/linux/wifi_direct_socket.h"

namespace nearby {
namespace linux {
std::string NetworkManagerWifiDirectServerSocket::GetIPAddress() const {
  auto ip4addresses = active_conn_->GetIP4Addresses();
  if (ip4addresses.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Could not find any IPv4 addresses for active connection "
        << active_conn_->getObjectPath();
    return std::string();
  }
  return ip4addresses[0];
}

int NetworkManagerWifiDirectServerSocket::GetPort() const {
  return server_socket_.GetPort();
}

std::unique_ptr<api::WifiDirectSocket>
NetworkManagerWifiDirectServerSocket::Accept() {
  auto sock = server_socket_.Accept();
  if (!sock.has_value()) return nullptr;
  return std::make_unique<WifiDirectSocket>(std::move(*sock));
}

Exception NetworkManagerWifiDirectServerSocket::Close() {
  return server_socket_.Close();
}
}  // namespace linux
}  // namespace nearby
