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

#include "core/internal/pcp_manager.h"

#include "core/internal/p2p_cluster_pcp_handler.h"
#include "core/internal/p2p_point_to_point_pcp_handler.h"
#include "core/internal/p2p_star_pcp_handler.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
PCPManager<Platform>::PCPManager(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager,
    Ptr<EndpointManager<Platform> > endpoint_manager,
    Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager)
    : pcp_handlers_(), current_pcp_handler_() {
  pcp_handlers_[PCP::P2P_CLUSTER] = MakePtr(new P2PClusterPCPHandler<Platform>(
      medium_manager, endpoint_manager, endpoint_channel_manager,
      bandwidth_upgrade_manager));
  pcp_handlers_[PCP::P2P_STAR] = MakePtr(new P2PStarPCPHandler<Platform>(
      medium_manager, endpoint_manager, endpoint_channel_manager,
      bandwidth_upgrade_manager));
  pcp_handlers_[PCP::P2P_POINT_TO_POINT] =
      MakePtr(new P2PPointToPointPCPHandler<Platform>(
          medium_manager, endpoint_manager, endpoint_channel_manager,
          bandwidth_upgrade_manager));
}

template <typename Platform>
PCPManager<Platform>::~PCPManager() {
  // TODO(tracyzhou): Add logging.

  // clear() instead of destroy() because this is just a reference -- the real
  // object will be destroyed in the loop below.
  current_pcp_handler_.clear();

  for (typename PCPHandlersMap::iterator it = pcp_handlers_.begin();
       it != pcp_handlers_.end(); it++) {
    it->second.destroy();
  }
  pcp_handlers_.clear();
}

template <typename Platform>
Status::Value PCPManager<Platform>::startAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
    const string& service_id, const AdvertisingOptions& advertising_options,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  if (!setCurrentPCPHandler(advertising_options.strategy)) {
    return Status::ERROR;
  }

  return current_pcp_handler_->startAdvertising(
      client_proxy, service_id, endpoint_name, advertising_options,
      connection_lifecycle_listener);
}

template <typename Platform>
void PCPManager<Platform>::stopAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy) {
  if (!current_pcp_handler_.isNull()) {
    current_pcp_handler_->stopAdvertising(client_proxy);
  }
}

template <typename Platform>
Status::Value PCPManager<Platform>::startDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
    const DiscoveryOptions& discovery_options,
    Ptr<DiscoveryListener> discovery_listener) {
  if (!setCurrentPCPHandler(discovery_options.strategy)) {
    return Status::ERROR;
  }

  return current_pcp_handler_->startDiscovery(
      client_proxy, service_id, discovery_options, discovery_listener);
}

template <typename Platform>
void PCPManager<Platform>::stopDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy) {
  if (!current_pcp_handler_.isNull()) {
    current_pcp_handler_->stopDiscovery(client_proxy);
  }
}

template <typename Platform>
Status::Value PCPManager<Platform>::requestConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
    const string& endpoint_id,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  if (current_pcp_handler_.isNull()) {
    return Status::OUT_OF_ORDER_API_CALL;
  }

  return current_pcp_handler_->requestConnection(
      client_proxy, endpoint_name, endpoint_id, connection_lifecycle_listener);
}

template <typename Platform>
Status::Value PCPManager<Platform>::acceptConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<PayloadListener> payload_listener) {
  if (current_pcp_handler_.isNull()) {
    return Status::OUT_OF_ORDER_API_CALL;
  }

  return current_pcp_handler_->acceptConnection(client_proxy, endpoint_id,
                                                payload_listener);
}

template <typename Platform>
Status::Value PCPManager<Platform>::rejectConnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id) {
  if (current_pcp_handler_.isNull()) {
    return Status::OUT_OF_ORDER_API_CALL;
  }

  return current_pcp_handler_->rejectConnection(client_proxy, endpoint_id);
}

template <typename Platform>
proto::connections::Medium PCPManager<Platform>::getBandwidthUpgradeMedium() {
  if (current_pcp_handler_.isNull()) {
    return proto::connections::Medium::UNKNOWN_MEDIUM;
  }

  return current_pcp_handler_->getBandwidthUpgradeMedium();
}

template <typename Platform>
bool PCPManager<Platform>::setCurrentPCPHandler(const Strategy& strategy) {
  current_pcp_handler_ = getPCPHandler(deducePCP(strategy));

  return !current_pcp_handler_.isNull();
}

template <typename Platform>
PCP::Value PCPManager<Platform>::deducePCP(const Strategy& strategy) {
  if (Strategy::kP2PCluster == strategy) {
    return PCP::P2P_CLUSTER;
  } else if (Strategy::kP2PStar == strategy) {
    return PCP::P2P_STAR;
  } else if (Strategy::kP2PPointToPoint == strategy) {
    return PCP::P2P_POINT_TO_POINT;
  } else {
    // TODO(tracyzhou): Add logging.
    return PCP::UNKNOWN;
  }
}

template <typename Platform>
Ptr<PCPHandler<Platform> > PCPManager<Platform>::getPCPHandler(PCP::Value pcp) {
  return pcp_handlers_[pcp];
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
