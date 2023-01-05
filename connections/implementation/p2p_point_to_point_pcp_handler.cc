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

#include "connections/implementation/p2p_point_to_point_pcp_handler.h"

#include <vector>

namespace nearby {
namespace connections {

P2pPointToPointPcpHandler::P2pPointToPointPcpHandler(
    Mediums& mediums, EndpointManager& endpoint_manager,
    EndpointChannelManager& channel_manager, BwuManager& bwu_manager,
    InjectedBluetoothDeviceStore& injected_bluetooth_device_store, Pcp pcp)
    : P2pStarPcpHandler(mediums, endpoint_manager, channel_manager, bwu_manager,
                        injected_bluetooth_device_store, pcp) {}

std::vector<location::nearby::proto::connections::Medium>
P2pPointToPointPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<location::nearby::proto::connections::Medium> mediums;
  if (mediums_->GetWifiLan().IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WIFI_LAN);
  }
  if (mediums_->GetWifi().IsAvailable() &&
      mediums_->GetWifiDirect().IsGCAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WIFI_DIRECT);
  }
  if (mediums_->GetWifi().IsAvailable() &&
      mediums_->GetWifiHotspot().IsClientAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WIFI_HOTSPOT);
  }
  if (mediums_->GetWebRtc().IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WEB_RTC);
  }
  if (mediums_->GetBluetoothClassic().IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::BLUETOOTH);
  }
  if (mediums_->GetBle().IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::BLE);
  }
  return mediums;
}

bool P2pPointToPointPcpHandler::CanSendOutgoingConnection(
    ClientProxy* client) const {
  // For point to point, we can only send an outgoing connection while we have
  // no other connections.
  return !this->HasOutgoingConnections(client) &&
         !this->HasIncomingConnections(client);
}

bool P2pPointToPointPcpHandler::CanReceiveIncomingConnection(
    ClientProxy* client) const {
  // For point to point, we can only receive an incoming connection while we
  // have no other connections.
  return !this->HasOutgoingConnections(client) &&
         !this->HasIncomingConnections(client);
}

}  // namespace connections
}  // namespace nearby
