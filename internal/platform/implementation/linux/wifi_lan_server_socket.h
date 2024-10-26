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

#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_SERVER_SOCKET_H_

#include <netinet/in.h>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_lan.h"

namespace nearby {
namespace linux {
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  explicit WifiLanServerSocket(int socket,
                               std::shared_ptr<NetworkManager> network_manager,
                               sdbus::IConnection &system_bus)
      : fd_(sdbus::UnixFd(socket)),
        network_manager_(std::move(network_manager)),
        system_bus_(system_bus) {}

  std::string GetIPAddress() const override;
  int GetPort() const override;

  std::unique_ptr<api::WifiLanSocket> Accept() override;
  Exception Close() override;

 private:
  sdbus::UnixFd fd_;
  std::shared_ptr<NetworkManager> network_manager_;
  sdbus::IConnection &system_bus_;
};
}  // namespace linux
}  // namespace nearby
#endif
