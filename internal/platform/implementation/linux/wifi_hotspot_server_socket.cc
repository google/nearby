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

#include "internal/platform/implementation/linux/wifi_hotspot_server_socket.h"
#include "internal/platform/implementation/linux/wifi_hotspot_socket.h"
#include "internal/platform/implementation/linux/wifi_medium.h"

namespace nearby {
namespace linux {
std::string NetworkManagerWifiHotspotServerSocket::GetIPAddress() const {
  auto ip4addresses = active_conn_->GetIP4Addresses();
  if (ip4addresses.empty()) {
    LOG(ERROR)
        << __func__
        << ": Could not find any IPv4 addresses for active connection "
        << active_conn_->getObjectPath();
    return {};
  }
  return ip4addresses[0];
}

int NetworkManagerWifiHotspotServerSocket::GetPort() const {
  struct sockaddr_in sin {};
  socklen_t len = sizeof(sin);
  auto ret =
      getsockname(fd_.get(), reinterpret_cast<struct sockaddr *>(&sin), &len);
  if (ret < 0) {
    LOG(ERROR) << __func__ << ": Error getting information for socket "
                       << fd_.get() << ": " << std::strerror(errno);
    return 0;
  }

  return ntohs(sin.sin_port);
}

std::unique_ptr<api::WifiHotspotSocket>
NetworkManagerWifiHotspotServerSocket::Accept() {
  struct sockaddr_in addr {};
  socklen_t len = sizeof(addr);

  auto conn =
      accept(fd_.get(), reinterpret_cast<struct sockaddr *>(&addr), &len);
  if (conn < 0) {
    LOG(ERROR) << __func__
                       << ": Error accepting incoming connections on socket "
                       << fd_.get() << ": " << std::strerror(errno);
    return nullptr;
  }

  return std::make_unique<WifiHotspotSocket>(conn);
}

Exception NetworkManagerWifiHotspotServerSocket::Close() {
  int fd = fd_.release();
  shutdown(fd, SHUT_RDWR);
  auto ret = close(fd_.release());
  if (ret < 0) {
    LOG(ERROR) << __func__
                       << ": Error closing socket: " << std::strerror(errno);
    return {Exception::kFailed};
  }

  return {Exception::kSuccess};
}
}  // namespace linux
}  // namespace nearby
