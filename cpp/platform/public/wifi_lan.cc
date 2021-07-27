// Copyright 2020 Google LLC
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

#include "platform/public/wifi_lan.h"

#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {

bool WifiLanMedium::StartAdvertising(const std::string& service_id,
                                     const NsdServiceInfo& nsd_service_info) {
  return impl_->StartAdvertising(service_id, nsd_service_info);
}

bool WifiLanMedium::StopAdvertising(const std::string& service_id) {
  return impl_->StopAdvertising(service_id);
}

bool WifiLanMedium::StartDiscovery(const std::string& service_id,
                                   DiscoveredServiceCallback callback) {
  {
    MutexLock lock(&mutex_);
    discovered_service_callback_ = std::move(callback);
    services_.clear();
  }
  return impl_->StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [this](api::WifiLanService& wifi_lan_service,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = services_.emplace(
                    &wifi_lan_service,
                    absl::make_unique<ServiceDiscoveryInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO,
                             "Discovering (again) service=%p, impl=%p, "
                             "service_info_name=%s",
                             &context.wifi_lan_service, &wifi_lan_service,
                             wifi_lan_service.GetServiceInfo()
                                 .GetServiceInfoName()
                                 .c_str());
                  return;
                } else {
                  context.wifi_lan_service = WifiLanService(&wifi_lan_service);
                  NEARBY_LOG(
                      INFO,
                      "Discovering wifi_lan_service=%p, service_info_name=%s",
                      &wifi_lan_service,
                      wifi_lan_service.GetServiceInfo()
                          .GetServiceInfoName()
                          .c_str());
                }
                discovered_service_callback_.service_discovered_cb(
                    context.wifi_lan_service, service_id);
              },
          .service_lost_cb =
              [this](api::WifiLanService& wifi_lan_service,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                if (services_.empty()) return;
                auto context = services_.find(&wifi_lan_service);
                if (context == services_.end()) return;
                NEARBY_LOG(INFO, "Removing wifi_lan_service=%p, impl=%p",
                           &(context->second->wifi_lan_service),
                           &wifi_lan_service);
                discovered_service_callback_.service_lost_cb(
                    context->second->wifi_lan_service, service_id);
              },
      });
}

bool WifiLanMedium::StopDiscovery(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    discovered_service_callback_ = {};
    services_.clear();
    NEARBY_LOG(INFO, "WifiLan Discovery disabled: impl=%p", &GetImpl());
  }
  return impl_->StopDiscovery(service_id);
}

bool WifiLanMedium::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = std::move(callback);
  }
  return impl_->StartAcceptingConnections(
      service_id,
      {
          .accepted_cb =
              [this](api::WifiLanSocket& socket,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = sockets_.emplace(
                    &socket, absl::make_unique<AcceptedConnectionInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO, "Accepting (again) socket=%p, impl=%p",
                             &context.socket, &socket);
                } else {
                  context.socket = WifiLanSocket(&socket);
                  NEARBY_LOG(INFO, "Accepting socket=%p, impl=%p",
                             &context.socket, &socket);
                }
                accepted_connection_callback_.accepted_cb(context.socket,
                                                          service_id);
              },
      });
}

bool WifiLanMedium::StopAcceptingConnections(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = {};
    sockets_.clear();
    NEARBY_LOG(INFO, "WifiLan accepted connection disabled: impl=%p",
               &GetImpl());
  }
  return impl_->StopAcceptingConnections(service_id);
}

WifiLanSocket WifiLanMedium::Connect(WifiLanService& wifi_lan_service,
                                     const std::string& service_id,
                                     CancellationFlag* cancellation_flag) {
  NEARBY_LOG(
      INFO,
      "WifiLanMedium::Connect: service=%p [impl=%p, service_info_name=%s]",
      &wifi_lan_service, &wifi_lan_service.GetImpl(),
      wifi_lan_service.GetServiceInfo().GetServiceInfoName().c_str());
  return WifiLanSocket(impl_->Connect(wifi_lan_service.GetImpl(), service_id,
                                      cancellation_flag));
}

WifiLanService WifiLanMedium::GetRemoteService(const std::string& ip_address,
                                               int port) {
  return WifiLanService(impl_->GetRemoteService(ip_address, port));
}

std::pair<std::string, int> WifiLanMedium::GetServiceAddress(
    const std::string& service_id) {
  return impl_->GetServiceAddress(service_id);
}

}  // namespace nearby
}  // namespace location
