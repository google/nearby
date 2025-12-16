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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

// Returns the first IP address.
std::string WifiLanServerSocket::GetIPAddress() const {
  // Just pick an IP address from the list of available addresses.
  std::vector<std::string> ip_addresses = GetIpv4Addresses();
  if (ip_addresses.empty()) {
    LOG(ERROR) << "No IP addresses found.";
    return "";
  }
  return ipaddr_dotdecimal_to_4bytes_string(ip_addresses.front());
}

// Blocks until either:
// - at least one incoming connection request is available, or
// - ServerSocket is closed.
// On success, returns connected socket, ready to exchange data.
// Returns nullptr on error.
// Once error is reported, it is permanent, and ServerSocket has to be closed.
std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  auto client_socket = server_socket_.Accept();
  if (client_socket == nullptr) {
    return nullptr;
  }

  LOG(INFO) << __func__ << ": Accepted a remote connection.";

  return std::make_unique<WifiLanSocket>(std::move(client_socket));
}

bool WifiLanServerSocket::Listen(int port) {
  // Listen on all interfaces.
  SocketAddress address;
  address.set_port(port);
  if (!server_socket_.Listen(address)) {
    LOG(ERROR) << "Failed to listen socket at port:" << port;
    return false;
  }
  return true;
}

}  // namespace nearby::windows
