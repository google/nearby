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

#ifndef PLATFORM_IMPL_LINUX_WIFI_DIRECT_H_
#define PLATFORM_IMPL_LINUX_WIFI_DIRECT_H_
#include <memory>

#include <optional>

#include <sdbus-c++/IConnection.h>

#include "internal/platform/implementation/linux/network_manager.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_direct.h"

namespace nearby {
namespace linux {
class NetworkManagerWifiDirectMedium : public api::WifiDirectMedium {
 public:
  NetworkManagerWifiDirectMedium(
      std::shared_ptr<networkmanager::NetworkManager> network_manager,
      std::unique_ptr<NetworkManagerWifiMedium> wireless_device)
      : system_bus_(network_manager->GetConnection()),
        network_manager_(std::move(network_manager)),
        wireless_device_(std::move(wireless_device)) {}

  bool IsInterfaceValid() const override { return true; }
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag *cancellation_flag) override;
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;
  bool ConnectWifiDirect(
      WifiDirectCredentials *wifi_direct_credentials) override;
  bool DisconnectWifiDirect() override;

  bool StartWifiDirect(WifiDirectCredentials *wifi_direct_credentials) override;
  bool StopWifiDirect() override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::nullopt;
  }

 private:
  bool ConnectedToWifi();

  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::shared_ptr<networkmanager::NetworkManager> network_manager_;
  std::unique_ptr<NetworkManagerWifiMedium> wireless_device_;
};
}  // namespace linux
}  // namespace nearby

#endif
