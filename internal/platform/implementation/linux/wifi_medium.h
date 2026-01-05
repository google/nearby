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

#ifndef PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_
#define PLATFORM_IMPL_LINUX_WIFI_MEDIUM_H_

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <ostream>

#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/ProxyInterfaces.h>
#include <sdbus-c++/StandardInterfaces.h>
#include <sdbus-c++/Types.h>

#include "absl/synchronization/mutex.h"
#include "absl/container/flat_hash_map.h"
#include "internal/platform/implementation/linux/generated/dbus/networkmanager/device_wireless_client.h"
#include "internal/platform/implementation/linux/network_manager.h"
#include "internal/platform/implementation/linux/network_manager_access_point.h"
#include "internal/platform/implementation/linux/network_manager_active_connection.h"
#include "internal/platform/implementation/wifi.h"

namespace nearby {
namespace linux {
class NetworkManagerWifiMedium
    : public api::WifiMedium,
      public sdbus::ProxyInterfaces<
          org::freedesktop::NetworkManager::Device::Wireless_proxy,
          sdbus::Properties_proxy> {
 public:
  NetworkManagerWifiMedium(const NetworkManagerWifiMedium &) = delete;
  NetworkManagerWifiMedium(NetworkManagerWifiMedium &&) = delete;
  NetworkManagerWifiMedium &operator=(const NetworkManagerWifiMedium &) =
      delete;
  NetworkManagerWifiMedium &operator=(NetworkManagerWifiMedium &&) = delete;
  NetworkManagerWifiMedium(
      std::shared_ptr<networkmanager::NetworkManager> network_manager,
      const sdbus::ObjectPath &wireless_device_object_path)
      : ProxyInterfaces(*network_manager->GetConnection(),
                        "org.freedesktop.NetworkManager",
                        wireless_device_object_path),
        system_bus_(network_manager->GetConnection()),
        network_manager_(std::move(network_manager)),
        last_scan_(-1) {
    registerProxy();
  }

  ~NetworkManagerWifiMedium() override { unregisterProxy(); }

  class ScanResultCallback : public api::WifiMedium::ScanResultCallback {
   public:
    void OnScanResults(
        const std::vector<api::WifiScanResult> &scan_results) override {
      // TODO: Add implementation at some point
    }
  };

  bool IsInterfaceValid() const override { return true; };
  api::WifiCapability &GetCapability() override;
  api::WifiInformation &GetInformation() override;
  bool Scan(
      const api::WifiMedium::ScanResultCallback &scan_result_callback) override;

  std::shared_ptr<NetworkManagerAccessPoint> SearchBySSID(
      absl::string_view ssid, absl::Duration scan_timeout = absl::Seconds(15))
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_);

  api::WifiConnectionStatus ConnectToNetwork(
      absl::string_view ssid, absl::string_view password,
      api::WifiAuthType auth_type) override;

  bool VerifyInternetConnectivity() override;
  std::string GetIpAddress() override;

  std::unique_ptr<networkmanager::ActiveConnection> GetActiveConnection();

 protected:
  void onPropertiesChanged(
      const std::string &interfaceName,
      const std::map<std::string, sdbus::Variant> &changedProperties,
      const std::vector<std::string> &invalidatedProperties) override;

  void onAccessPointAdded(const sdbus::ObjectPath &access_point) override
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_) {
    absl::MutexLock l(&known_access_points_lock_);
    known_access_points_.erase(access_point);
    known_access_points_.emplace(access_point,
                                 std::make_shared<NetworkManagerAccessPoint>(
                                     getProxy().getConnection(), access_point));
  }
  void onAccessPointRemoved(const sdbus::ObjectPath &access_point) override
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_) {
    absl::MutexLock l(&known_access_points_lock_);
    known_access_points_.erase(access_point);
  }

 private:
  std::shared_ptr<NetworkManagerAccessPoint> SearchBySSIDNoScan(
      std::vector<std::uint8_t> &ssid)
      ABSL_LOCKS_EXCLUDED(known_access_points_lock_);

  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::shared_ptr<networkmanager::NetworkManager> network_manager_;

  api::WifiCapability capability_;
  api::WifiInformation information_{false};

  absl::Mutex known_access_points_lock_;
  absl::flat_hash_map<sdbus::ObjectPath,
                      std::shared_ptr<NetworkManagerAccessPoint>>
      known_access_points_ ABSL_GUARDED_BY(known_access_points_lock_);

  absl::Mutex scan_result_callback_lock_;
  std::optional<
      std::reference_wrapper<const api::WifiMedium::ScanResultCallback>>
      scan_result_callback_ ABSL_GUARDED_BY(scan_result_callback_lock_);

  absl::Mutex last_scan_lock_;
  std::int64_t last_scan_ ABSL_GUARDED_BY(last_scan_lock_);
};

}  // namespace linux
}  // namespace nearby

#endif
