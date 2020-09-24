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

#include "core_v2/internal/pcp_manager.h"

#include "core_v2/internal/p2p_cluster_pcp_handler.h"
#include "core_v2/internal/p2p_point_to_point_pcp_handler.h"
#include "core_v2/internal/p2p_star_pcp_handler.h"
#include "core_v2/internal/pcp_handler.h"

namespace location {
namespace nearby {
namespace connections {

PcpManager::PcpManager(Mediums& mediums,
                       EndpointChannelManager& channel_manager,
                       EndpointManager& endpoint_manager,
                       BwuManager& bwu_manager) {
  handlers_[Pcp::kP2pCluster] = std::make_unique<P2pClusterPcpHandler>(
      &mediums, &endpoint_manager, &channel_manager, &bwu_manager);
  handlers_[Pcp::kP2pStar] = std::make_unique<P2pStarPcpHandler>(
      mediums, endpoint_manager, channel_manager, bwu_manager);
  handlers_[Pcp::kP2pPointToPoint] =
      std::make_unique<P2pPointToPointPcpHandler>(mediums, endpoint_manager,
                                                  channel_manager, bwu_manager);
}

void PcpManager::DisconnectFromEndpointManager() {
  if (shutdown_.Set(true)) return;
  for (auto& item : handlers_) {
    if (!item.second) continue;
    item.second->DisconnectFromEndpointManager();
  }
}

PcpManager::~PcpManager() {
  DisconnectFromEndpointManager();
}

Status PcpManager::StartAdvertising(ClientProxy* client,
                                    const string& service_id,
                                    const ConnectionOptions& options,
                                    const ConnectionRequestInfo& info) {
  if (!SetCurrentPcpHandler(options.strategy)) {
    return {Status::kError};
  }

  return current_->StartAdvertising(client, service_id, options, info);
}

void PcpManager::StopAdvertising(ClientProxy* client) {
  if (current_) {
    current_->StopAdvertising(client);
  }
}

Status PcpManager::StartDiscovery(ClientProxy* client, const string& service_id,
                                  const ConnectionOptions& options,
                                  DiscoveryListener listener) {
  if (!SetCurrentPcpHandler(options.strategy)) {
    return {Status::kError};
  }

  return current_->StartDiscovery(client, service_id, options,
                                  std::move(listener));
}

void PcpManager::StopDiscovery(ClientProxy* client) {
  if (current_) {
    current_->StopDiscovery(client);
  }
}

Status PcpManager::RequestConnection(ClientProxy* client,
                                     const string& endpoint_id,
                                     const ConnectionRequestInfo& info,
                                     const ConnectionOptions& options) {
  if (!current_) {
    return {Status::kOutOfOrderApiCall};
  }

  return current_->RequestConnection(client, endpoint_id, info, options);
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
}  // namespace location
