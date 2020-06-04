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

#ifndef CORE_INTERNAL_BANDWIDTH_UPGRADE_HANDLER_H_
#define CORE_INTERNAL_BANDWIDTH_UPGRADE_HANDLER_H_

#include "core/internal/client_proxy.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/platform.h"
#include "platform/port/string.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class BandwidthUpgradeHandler {
 public:
  using Platform = platform::ImplementationPlatform;

  virtual ~BandwidthUpgradeHandler() {}

  // Reverts any changes made to the device in the process of upgrading
  // endpoints.
  virtual void revert() = 0;

  // Cleans up in-progress upgrades after endpoint disconnection.
  virtual void processEndpointDisconnection(
      Ptr<ClientProxy<Platform> > client_proxy, const std::string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier) = 0;

  // Initiates the upgrade for the endpoint and starts listening for upgraded
  // incoming connections on the initiator side of the bandwidth upgrade.
  virtual void initiateBandwidthUpgradeForEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::string& endpoint_id) = 0;

  // Processes the BandwidthUpgradeNegotiationFrames that come over the
  // EndpointChannel on the non-initiator side of the bandwidth upgrade.
  // TODO(ahlee): Rename parameters in the java code.
  virtual void processBandwidthUpgradeNegotiationFrame(
      ConstPtr<BandwidthUpgradeNegotiationFrame> bandwidth_upgrade_negotiation,
      Ptr<ClientProxy<Platform> > to_client_proxy,
      const std::string& from_endpoint_id,
      proto::connections::Medium current_medium) = 0;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BANDWIDTH_UPGRADE_HANDLER_H_
