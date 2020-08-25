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

#include "platform_v2/public/wifi_lan.h"

#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex_lock.h"

namespace location {
namespace nearby {

bool WifiLanMedium::StartAdvertising(const std::string& service_id,
                                     const std::string& service_info_name) {
  return impl_->StartAdvertising(service_id, service_info_name);
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
              [this](api::WifiLanService& service,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = services_.emplace(
                    &service, absl::make_unique<ServiceDiscoveryInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO,
                             "Discovering (again) service=%p, impl=%p, "
                             "service_info_name=%s",
                             &context.service, &service,
                             service.GetName().c_str());
                } else {
                  context.service = WifiLanService(&service);
                  NEARBY_LOG(
                      INFO,
                      "Discovering service=%p, impl=%p, service_info_name=%s",
                      &context.service, &service, service.GetName().c_str());
                }
                discovered_service_callback_.service_discovered_cb(
                    context.service, service_id);
              },
          .service_lost_cb =
              [this](api::WifiLanService& service,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                if (services_.empty()) return;
                auto context = services_.find(&service);
                if (context == services_.end()) return;
                NEARBY_LOG(INFO, "Removing service=%p, impl=%p",
                           &(context->second->service), &service);
                discovered_service_callback_.service_lost_cb(
                    context->second->service, service_id);
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

WifiLanSocket WifiLanMedium::Connect(WifiLanService& service,
                                     const std::string& service_id) {
  NEARBY_LOG(
      INFO,
      "WifiLanMedium::Connect: service=%p [impl=%p, service_info_name=%s]",
      &service, &service.GetImpl(), service.GetName().c_str());
  return WifiLanSocket(impl_->Connect(service.GetImpl(), service_id));
}

WifiLanService WifiLanMedium::FindRemoteService(const std::string& ip_address,
                                                int port) {
  return WifiLanService(impl_->FindRemoteService(ip_address, port));
}

}  // namespace nearby
}  // namespace location
