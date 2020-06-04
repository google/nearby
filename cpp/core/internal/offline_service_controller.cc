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

#include "core/internal/offline_service_controller.h"

#include <string>

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
OfflineServiceController<Platform>::OfflineServiceController()
    : ServiceController<Platform>(),
      medium_manager_(new MediumManager<Platform>()),
      endpoint_channel_manager_(
          new EndpointChannelManager(medium_manager_.get())),
      endpoint_manager_(
          new EndpointManager<Platform>(endpoint_channel_manager_.get())),
      payload_manager_(new PayloadManager<Platform>(endpoint_manager_.get())),
      bandwidth_upgrade_manager_(new BandwidthUpgradeManager(
          medium_manager_.get(), endpoint_channel_manager_.get(),
          endpoint_manager_.get())),
      pcp_manager_(new PCPManager<Platform>(
          medium_manager_.get(), endpoint_channel_manager_.get(),
          endpoint_manager_.get(), bandwidth_upgrade_manager_.get())) {}

template <typename Platform>
OfflineServiceController<Platform>::~OfflineServiceController() {}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::startAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
    const string& service_id, const AdvertisingOptions& advertising_options,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  return pcp_manager_->startAdvertising(client_proxy, endpoint_name, service_id,
                                        advertising_options,
                                        connection_lifecycle_listener);
}

template <typename Platform>
void OfflineServiceController<Platform>::stopAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy) {
  pcp_manager_->stopAdvertising(client_proxy);
}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::startDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
    const DiscoveryOptions& discovery_options,
    Ptr<DiscoveryListener> discovery_listener) {
  return pcp_manager_->startDiscovery(client_proxy, service_id,
                                      discovery_options, discovery_listener);
}

template <typename Platform>
void OfflineServiceController<Platform>::stopDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy) {
  pcp_manager_->stopDiscovery(client_proxy);
}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::requestConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
    const string& endpoint_id,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  return pcp_manager_->requestConnection(
      client_proxy, endpoint_name, endpoint_id, connection_lifecycle_listener);
}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::acceptConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<PayloadListener> payload_listener) {
  return pcp_manager_->acceptConnection(client_proxy, endpoint_id,
                                        payload_listener);
}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::rejectConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {
  return pcp_manager_->rejectConnection(client_proxy, endpoint_id);
}

template <typename Platform>
void OfflineServiceController<Platform>::initiateBandwidthUpgrade(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {
  bandwidth_upgrade_manager_->initiateBandwidthUpgradeForEndpoint(
      client_proxy, endpoint_id, pcp_manager_->getBandwidthUpgradeMedium());
}

template <typename Platform>
void OfflineServiceController<Platform>::sendPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    const std::vector<string>& endpoint_ids, ConstPtr<Payload> payload) {
  payload_manager_->sendPayload(client_proxy, endpoint_ids, payload);
}

template <typename Platform>
Status::Value OfflineServiceController<Platform>::cancelPayload(
    Ptr<ClientProxy<Platform> > client_proxy, std::int64_t payload_id) {
  return payload_manager_->cancelPayload(client_proxy, payload_id);
}

template <typename Platform>
void OfflineServiceController<Platform>::disconnectFromEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {
  endpoint_manager_->unregisterEndpoint(client_proxy, endpoint_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
