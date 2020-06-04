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

#include "core/internal/bandwidth_upgrade_manager.h"

#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

BandwidthUpgradeManager::BandwidthUpgradeManager(
    Ptr<MediumManager<Platform> > medium_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager,
    Ptr<EndpointManager<Platform> > endpoint_manager)
    : endpoint_manager_(endpoint_manager),
      bandwidth_upgrade_handlers_(),
      current_bandwidth_upgrade_handler_() {}

BandwidthUpgradeManager::~BandwidthUpgradeManager() {
  // TODO(ahlee): Make sure we don't repeat the mistake fixed in cl/201883908.
}

void BandwidthUpgradeManager::initiateBandwidthUpgradeForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    proto::connections::Medium medium) {}

void BandwidthUpgradeManager::processIncomingOfflineFrame(
    ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
    Ptr<ClientProxy<Platform> > to_client_proxy,
    proto::connections::Medium current_medium) {}

void BandwidthUpgradeManager::processEndpointDisconnection(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {}

bool BandwidthUpgradeManager::setCurrentBandwidthUpgradeHandler(
    proto::connections::Medium medium) {
  return false;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
