// Copyright 2025 Google LLC
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
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {

bool WifiAwareMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  if (!IsValid()) {
    return false;
  }
  return impl_->StartAdvertising(nsd_service_info);
}

bool WifiAwareMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  if (!IsValid()) {
    return false;
  }
  return impl_->StopAdvertising(nsd_service_info);
}

bool WifiAwareMedium::StartDiscovery(const std::string& service_type,
                                     DiscoveredServiceCallback callback) {
  if (!IsValid()) {
    return false;
  }
  {
    MutexLock lock(&mutex_);
    if (service_type_to_callback_map_.contains(service_type)) {
      LOG(INFO) << "WifiAware Discovery already start with service_type="
                << service_type << "; impl=" << GetImpl();
      return false;
    }
  }
  api::WifiAwareMedium::DiscoveredServiceCallback api_callback = {
      .service_discovered_cb =
          [this](const NsdServiceInfo& service_info) {
            MutexLock lock(&mutex_);
            std::string service_type = service_info.GetServiceType();
            // Check callback for the service type.
            const auto& it = service_type_to_callback_map_.find(service_type);

            if (it == service_type_to_callback_map_.end()) {
              LOG(ERROR) << "There is no callback found for service_type="
                         << service_type;
              return;
            }

            // Check whether service name is in cache.
            auto services_it = service_type_to_services_map_.find(service_type);
            if (services_it == service_type_to_services_map_.end()) {
              LOG(ERROR) << "There is no service map found for service_type="
                         << service_type;
              return;
            }

            std::string service_name = service_info.GetServiceName();
            auto pair = services_it->second.insert(service_name);
            if (!pair.second) {
              LOG(INFO) << "Discovering (again) service_info=" << &service_info
                        << ", service_type=" << service_type
                        << ", service_name=" << service_info.GetServiceName();
              return;
            }

            LOG(INFO) << "Adding service_info=" << &service_info
                      << ", service_type=" << service_type
                      << ", service_name=" << service_info.GetServiceName();

            DiscoveredServiceCallback& medium_callback =
                it->second->medium_callback;
            medium_callback.service_discovered_cb(service_info, service_type);
          },
      .service_lost_cb =
          [this](const NsdServiceInfo& service_info) {
            MutexLock lock(&mutex_);
            std::string service_type = service_info.GetServiceType();
            std::string service_name = service_info.GetServiceName();
            auto services_it = service_type_to_services_map_.find(service_type);
            if (services_it == service_type_to_services_map_.end()) {
              LOG(ERROR) << "There is no service map found for service_type="
                         << service_type;
              return;
            }

            auto item = services_it->second.extract(service_name);
            if (item.empty()) return;
            LOG(INFO) << "Removing service_info=" << &service_info
                      << ", service_type=" << service_type
                      << ", service_info_name=" << service_name;
            // Callback service lost.
            const auto& it = service_type_to_callback_map_.find(service_type);
            if (it != service_type_to_callback_map_.end()) {
              DiscoveredServiceCallback& medium_callback =
                  it->second->medium_callback;
              medium_callback.service_lost_cb(service_info, service_type);
            }
          },
  };
  {
    // Insert callback to the map first no matter it succeeds or not.
    MutexLock lock(&mutex_);
    auto pair = service_type_to_callback_map_.insert(
        {service_type, std::make_unique<DiscoveryCallbackInfo>()});
    auto& context = *pair.first->second;
    context.medium_callback = std::move(callback);

    // Insert an empty services set to track the services under the service
    // type.
    service_type_to_services_map_.insert(
        {service_type, absl::flat_hash_set<std::string>()});
  }

  bool success = impl_->StartDiscovery(service_type, std::move(api_callback));
  if (!success) {
    // If failed, then revert back the insertion.
    MutexLock lock(&mutex_);
    service_type_to_callback_map_.erase(service_type);
    service_type_to_services_map_.erase(service_type);
  }
  LOG(INFO) << "WifiAware Discovery started for service_type=" << service_type
            << ", impl=" << GetImpl() << ", success=" << success;
  return success;
}

bool WifiAwareMedium::StopDiscovery(const std::string& service_type) {
  if (!IsValid()) {
    return false;
  }
  MutexLock lock(&mutex_);
  if (service_type_to_callback_map_.erase(service_type) == 0) {
    return false;
  }
  service_type_to_services_map_.erase(service_type);
  LOG(INFO) << "WifiAware Discovery disabled for service_type=" << service_type
            << ", impl=" << GetImpl();
  return impl_->StopDiscovery(service_type);
}

WifiAwareSocket WifiAwareMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
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
