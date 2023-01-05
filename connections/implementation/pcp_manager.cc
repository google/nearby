// Copyright 2020-2021 Google LLC
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

#include "connections/implementation/pcp_manager.h"

#include "connections/implementation/p2p_cluster_pcp_handler.h"
#include "connections/implementation/p2p_point_to_point_pcp_handler.h"
#include "connections/implementation/p2p_star_pcp_handler.h"
#include "connections/implementation/pcp_handler.h"

namespace nearby {
namespace connections {

PcpManager::PcpManager(
    Mediums& mediums, EndpointChannelManager& channel_manager,
    EndpointManager& endpoint_manager, BwuManager& bwu_manager,
    InjectedBluetoothDeviceStore& injected_bluetooth_device_store) {
  handlers_[Pcp::kP2pCluster] = std::make_unique<P2pClusterPcpHandler>(
      &mediums, &endpoint_manager, &channel_manager, &bwu_manager,
      injected_bluetooth_device_store);
  handlers_[Pcp::kP2pStar] = std::make_unique<P2pStarPcpHandler>(
      mediums, endpoint_manager, channel_manager, bwu_manager,
      injected_bluetooth_device_store);
  handlers_[Pcp::kP2pPointToPoint] =
      std::make_unique<P2pPointToPointPcpHandler>(
          mediums, endpoint_manager, channel_manager, bwu_manager,
          injected_bluetooth_device_store);
}

void PcpManager::DisconnectFromEndpointManager() {
  if (shutdown_.Set(true)) return;
  for (auto& item : handlers_) {
    if (!item.second) continue;
    item.second->DisconnectFromEndpointManager();
  }
}

PcpManager::~PcpManager() {
  NEARBY_LOGS(INFO) << "Initiating shutdown of PcpManager.";
  DisconnectFromEndpointManager();
  NEARBY_LOGS(INFO) << "PcpManager has shut down.";
}

Status PcpManager::StartAdvertising(
    ClientProxy* client, const string& service_id,
    const AdvertisingOptions& advertising_options,
    const ConnectionRequestInfo& info) {
  if (!SetCurrentPcpHandler(advertising_options.strategy)) {
    return {Status::kError};
  }

  return current_->StartAdvertising(client, service_id, advertising_options,
                                    info);
}

void PcpManager::StopAdvertising(ClientProxy* client) {
  if (current_) {
    current_->StopAdvertising(client);
  }
}

Status PcpManager::StartDiscovery(ClientProxy* client, const string& service_id,
                                  const DiscoveryOptions& discovery_options,
                                  DiscoveryListener listener) {
  if (!SetCurrentPcpHandler(discovery_options.strategy)) {
    return {Status::kError};
  }

  return current_->StartDiscovery(client, service_id, discovery_options,
                                  std::move(listener));
}

void PcpManager::StopDiscovery(ClientProxy* client) {
  if (current_) {
    current_->StopDiscovery(client);
  }
}

void PcpManager::InjectEndpoint(ClientProxy* client,
                                const std::string& service_id,
                                const OutOfBandConnectionMetadata& metadata) {
  if (current_) {
    current_->InjectEndpoint(client, service_id, metadata);
  }
}

Status PcpManager::RequestConnection(
    ClientProxy* client, const string& endpoint_id,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  if (!current_) {
    return {Status::kOutOfOrderApiCall};
  }

  return current_->RequestConnection(client, endpoint_id, info,
                                     connection_options);
}

Status PcpManager::AcceptConnection(ClientProxy* client,
                                    const string& endpoint_id,
                                    const PayloadListener& payload_listener) {
  if (!current_) {
    return {Status::kOutOfOrderApiCall};
  }

  return current_->AcceptConnection(client, endpoint_id, payload_listener);
}

Status PcpManager::RejectConnection(ClientProxy* client,
                                    const string& endpoint_id) {
  if (!current_) {
    return {Status::kOutOfOrderApiCall};
  }

  return current_->RejectConnection(client, endpoint_id);
}

bool PcpManager::SetCurrentPcpHandler(Strategy strategy) {
  current_ = GetPcpHandler(StrategyToPcp(strategy));

  if (!current_) {
    NEARBY_LOG(ERROR, "Failed to set current PCP handler: strategy=%s",
               strategy.GetName().c_str());
  }

  return current_;
}

PcpHandler* PcpManager::GetPcpHandler(Pcp pcp) const {
  auto item = handlers_.find(pcp);
  return item != handlers_.end() ? item->second.get() : nullptr;
}

}  // namespace connections
}  // namespace nearby
