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

#ifndef PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_H_

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_hotspot.h"

namespace nearby {
namespace linux {
class NetworkManagerWifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  NetworkManagerWifiHotspotMedium(
      std::shared_ptr<networkmanager::NetworkManager> network_manager,
      sdbus::ObjectPath wireless_device_object_path)
      : system_bus_(network_manager->GetConnection()),
        wireless_device_(std::make_unique<NetworkManagerWifiMedium>(
            network_manager, std::move(wireless_device_object_path))),
        network_manager_(std::move(network_manager)) {}
  NetworkManagerWifiHotspotMedium(
      std::shared_ptr<networkmanager::NetworkManager> network_manager,
      std::unique_ptr<NetworkManagerWifiMedium> wireless_device)
      : system_bus_(network_manager->GetConnection()),
        wireless_device_(std::move(wireless_device)),
        network_manager_(std::move(network_manager)) {}
  
  bool IsInterfaceValid() const override { return true; }
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag *cancellation_flag) override;
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  bool StartWifiHotspot(HotspotCredentials *hotspot_credentials) override;
  bool StopWifiHotspot() override;

  bool ConnectWifiHotspot(HotspotCredentials *hotspot_credentials) override;
  bool DisconnectWifiHotspot() override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return absl::nullopt;
  }

 private:
  bool WifiHotspotActive();
  bool ConnectedToWifi();

  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::unique_ptr<NetworkManagerWifiMedium> wireless_device_;
  std::shared_ptr<networkmanager::NetworkManager> network_manager_;
};
}  // namespace linux
}  // namespace nearby

#endif
