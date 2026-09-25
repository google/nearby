// Copyright 2026 Google LLC
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

#include "internal/platform/wifi_aware.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/wifi_aware_service_info.h"

namespace nearby {

bool WifiAwareMedium::StartAdvertising(
    const WifiAwareServiceInfo& wifi_aware_service_info) {
  if (!IsValid()) {
    return false;
  }
  return impl_->StartAdvertising(wifi_aware_service_info);
}

bool WifiAwareMedium::StopAdvertising(
    const WifiAwareServiceInfo& wifi_aware_service_info) {
  if (!IsValid()) {
    return false;
  }
  return impl_->StopAdvertising(wifi_aware_service_info);
}

bool WifiAwareMedium::StartDiscovery(absl::string_view service_type,
                                     DiscoveredServiceCallback callback) {
  if (!IsValid()) {
    return false;
  }
  auto callback_ptr =
      std::make_shared<DiscoveredServiceCallback>(std::move(callback));
  api::WifiAwareMedium::DiscoveredServiceCallback api_callback = {
      .service_discovered_cb =
          [this](const WifiAwareServiceInfo& service_info) {
            std::shared_ptr<DiscoveredServiceCallback> cb;
            std::string service_type = service_info.GetServiceType();
            {
              MutexLock lock(&mutex_);
              auto it = service_type_to_callback_info_map_.find(service_type);
              if (it == service_type_to_callback_info_map_.end()) {
                LOG(ERROR) << "There is no callback found for service_type="
                           << service_type;
                return;
              }

              std::string service_name = service_info.GetServiceName();
              auto pair = it->second.services.insert(service_name);
              if (!pair.second) {
                LOG(INFO) << "Discovering (again) service_name=" << service_name
                          << ", service_type=" << service_type;
                return;
              }

              LOG(INFO) << "Adding service_name=" << service_name
                        << ", service_type=" << service_type;
              cb = it->second.medium_callback;
            }

            if (cb && cb->service_discovered_cb) {
              cb->service_discovered_cb(service_info, service_type);
            }
          },
      .service_lost_cb =
          [this](const WifiAwareServiceInfo& service_info) {
            std::shared_ptr<DiscoveredServiceCallback> cb;
            std::string service_type = service_info.GetServiceType();
            {
              MutexLock lock(&mutex_);
              auto it = service_type_to_callback_info_map_.find(service_type);
              if (it == service_type_to_callback_info_map_.end()) {
                LOG(ERROR) << "There is no callback found for service_type="
                           << service_type;
                return;
              }

              std::string service_name = service_info.GetServiceName();
              if (it->second.services.erase(service_name) == 0) return;
              LOG(INFO) << "Removing service_name=" << service_name
                        << ", service_type=" << service_type;
              cb = it->second.medium_callback;
            }

            if (cb && cb->service_lost_cb) {
              cb->service_lost_cb(service_info, service_type);
            }
          },
  };

  {
    MutexLock lock(&mutex_);
    auto [it, inserted] = service_type_to_callback_info_map_.emplace(
        service_type, DiscoveryCallbackInfo{.medium_callback = callback_ptr});
    if (!inserted) {
      LOG(INFO) << "WifiAware Discovery already started with service_type="
                << service_type;
      return false;
    }
  }

  bool success =
      impl_->StartDiscovery(std::string(service_type), std::move(api_callback));
  if (!success) {
    // If failed, then revert back the insertion.
    MutexLock lock(&mutex_);
    service_type_to_callback_info_map_.erase(service_type);
  }
  LOG(INFO) << "WifiAware Discovery started for service_type=" << service_type
            << ", success=" << success;
  return success;
}

bool WifiAwareMedium::StopDiscovery(absl::string_view service_type) {
  if (!IsValid()) {
    return false;
  }
  {
    MutexLock lock(&mutex_);
    if (service_type_to_callback_info_map_.erase(service_type) == 0) {
      return false;
    }
  }
  LOG(INFO) << "WifiAware Discovery disabled for service_type=" << service_type;
  return impl_->StopDiscovery(std::string(service_type));
}

WifiAwareSocket WifiAwareMedium::ConnectToService(
    const WifiAwareServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  if (!IsValid()) {
    return WifiAwareSocket();
  }
  LOG(INFO) << "WifiAwareMedium::ConnectToService: remote_service_name="
            << remote_service_info.GetServiceName();
  return WifiAwareSocket(
      impl_->ConnectToService(remote_service_info, cancellation_flag));
}

WifiAwareServerSocket WifiAwareMedium::ListenForService(int port) {
  if (!IsValid()) {
    return WifiAwareServerSocket();
  }
  return WifiAwareServerSocket(impl_->ListenForService(port));
}

api::UpgradeAddressInfo WifiAwareMedium::GetUpgradeAddressCandidates(
    const WifiAwareServerSocket& server_socket) {
  return api::UpgradeAddressInfo();
}

}  // namespace nearby
