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

#include "sharing/internal/public/connectivity_manager_impl.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/nearby_flags.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/api/network_monitor.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace {

using ::nearby::sharing::api::SharingPlatform;
using ConnectionType = ConnectivityManager::ConnectionType;

std::string GetConnectionTypeString(ConnectionType connection_type) {
  switch (connection_type) {
    case ConnectionType::k2G:
      return "2G";
    case ConnectionType::k3G:
      return "3G";
    case ConnectionType::k4G:
      return "4G";
    case ConnectionType::k5G:
      return "5G";
    case ConnectionType::kBluetooth:
      return "Bluetooth";
    case ConnectionType::kEthernet:
      return "Ethernet";
    case ConnectionType::kWifi:
      return "WiFi";
    default:
      return "Unknown";
  }
}

}  // namespace

ConnectivityManagerImpl::ConnectivityManagerImpl(SharingPlatform& platform)
    : platform_(platform) {
  network_monitor_ = platform.CreateNetworkMonitor(
      [this](api::NetworkMonitor::ConnectionType connection_type,
             bool is_lan_connected, bool is_internet_connected) {
        ConnectionType new_connection_type =
            static_cast<ConnectionType>(connection_type);
        VLOG(1) << ": New connection type:"
                << GetConnectionTypeString(new_connection_type);
        for (auto& listener : listeners_) {
          listener.second(new_connection_type, is_lan_connected,
                          is_internet_connected);
        }
      });
}

bool ConnectivityManagerImpl::IsLanConnected() {
  return network_monitor_->IsLanConnected();
}

bool ConnectivityManagerImpl::IsInternetConnected() {
  return network_monitor_->IsInternetConnected();
}

bool ConnectivityManagerImpl::IsHPRealtekDevice() {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          sharing::config_package_nearby::nearby_sharing_feature::
              kEnableWifiHotspotForHpRealtekDevices)) {
    return false;
  }

  absl::MutexLock lock(mutex_);

  // Lazy initialization since CreateSystemInfo can take a long time.
  if (!is_hp_realtek_device_.has_value()) {
    auto system_info = platform_.CreateSystemInfo();
    const auto& driver_infos = system_info->GetNetworkDriverInfos();
    auto it = std::find_if(driver_infos.begin(), driver_infos.end(),
                           [](auto drive_info) {
                             return absl::StrContainsIgnoreCase(
                                 drive_info.manufacturer, "Realtek");
                           });
    if (it != driver_infos.end() &&
        absl::EqualsIgnoreCase(system_info->GetComputerManufacturer(), "HP")) {
      is_hp_realtek_device_ = std::make_optional(true);
    } else {
      is_hp_realtek_device_ = std::make_optional(false);
    }
  }
  return is_hp_realtek_device_.value();
}

ConnectionType ConnectivityManagerImpl::GetConnectionType() {
  return static_cast<ConnectionType>(network_monitor_->GetCurrentConnection());
}

void ConnectivityManagerImpl::RegisterConnectionListener(
    absl::string_view listener_name,
    std::function<void(ConnectionType, bool, bool)> callback) {
  listeners_.emplace(listener_name, std::move(callback));
}

void ConnectivityManagerImpl::UnregisterConnectionListener(
    absl::string_view listener_name) {
  listeners_.erase(listener_name);
}

int ConnectivityManagerImpl::GetListenerCount() const {
  return listeners_.size();
}

}  // namespace nearby
