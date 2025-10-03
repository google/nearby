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
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/flags/nearby_flags.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/api/network_monitor.h"
#include "sharing/internal/api/sharing_platform.h"

namespace nearby {
namespace {

using ::nearby::sharing::api::SharingPlatform;

}  // namespace

ConnectivityManagerImpl::ConnectivityManagerImpl(SharingPlatform& platform)
    : platform_(platform) {
  network_monitor_ = platform.CreateNetworkMonitor(
      [this](bool is_lan_connected) {
        absl::MutexLock lock(mutex_);
        for (auto& listener : lan_listeners_) {
          listener.second(is_lan_connected);
        }
      },
      [this](bool is_internet_connected) {
        absl::MutexLock lock(mutex_);
        for (auto& listener : internet_listeners_) {
          listener.second(is_internet_connected);
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

void ConnectivityManagerImpl::RegisterLanListener(
    absl::string_view listener_name, std::function<void(bool)> callback) {
  absl::MutexLock lock(mutex_);
  lan_listeners_.emplace(listener_name, std::move(callback));
}

void ConnectivityManagerImpl::UnregisterLanListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(mutex_);
  lan_listeners_.erase(listener_name);
}

void ConnectivityManagerImpl::RegisterInternetListener(
    absl::string_view listener_name, std::function<void(bool)> callback) {
  absl::MutexLock lock(mutex_);
  internet_listeners_.emplace(listener_name, std::move(callback));
}

void ConnectivityManagerImpl::UnregisterInternetListener(
    absl::string_view listener_name) {
  absl::MutexLock lock(mutex_);
  internet_listeners_.erase(listener_name);
}

int ConnectivityManagerImpl::GetLanListenerCountForTests() const {
  absl::MutexLock lock(mutex_);
  return lan_listeners_.size();
}
int ConnectivityManagerImpl::GetInternetListenerCountForTests() const {
  absl::MutexLock lock(mutex_);
  return internet_listeners_.size();
}

}  // namespace nearby
