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

#ifndef CORE_INTERNAL_PCP_MANAGER_H_
#define CORE_INTERNAL_PCP_MANAGER_H_

#include <map>

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "core/internal/pcp_handler.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/status.h"
#include "core/strategy.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Manages all known PCPHandler implementations, delegating operations to the
// appropriate one as per the parameters passed in.
//
// <p>This will only ever be used by the OfflineServiceController, which has all
// of its entrypoints invoked serially, so there's no synchronization needed.
template <typename Platform>
class PCPManager {
 public:
  PCPManager(Ptr<MediumManager<Platform> > medium_manager,
             Ptr<EndpointChannelManager> endpoint_channel_manager,
             Ptr<EndpointManager<Platform> > endpoint_manager,
             Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager);
  ~PCPManager();

  Status::Value startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
      const string& service_id, const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener);
  void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy);

  Status::Value startDiscovery(Ptr<ClientProxy<Platform> > client_proxy,
                               const string& service_id,
                               const DiscoveryOptions& discovery_options,
                               Ptr<DiscoveryListener> discovery_listener);
  void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy);

  Status::Value requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
      const string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener);
  Status::Value acceptConnection(Ptr<ClientProxy<Platform> > client_proxy,
                                 const string& endpoint_id,
                                 Ptr<PayloadListener> payload_listener);
  Status::Value rejectConnection(Ptr<ClientProxy<Platform> > client_proxy,
                                 const string& endpoint_id);

  proto::connections::Medium getBandwidthUpgradeMedium();

 private:
  bool setCurrentPCPHandler(const Strategy& strategy);
  PCP::Value deducePCP(const Strategy& strategy);
  Ptr<PCPHandler<Platform> > getPCPHandler(PCP::Value pcp);

  typedef std::map<PCP::Value, Ptr<PCPHandler<Platform> > > PCPHandlersMap;
  PCPHandlersMap pcp_handlers_;
  Ptr<PCPHandler<Platform> > current_pcp_handler_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/pcp_manager.cc"

#endif  // CORE_INTERNAL_PCP_MANAGER_H_
