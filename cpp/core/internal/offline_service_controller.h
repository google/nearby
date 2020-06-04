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

#ifndef CORE_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_
#define CORE_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_

#include <cstdint>
#include <vector>

#include "core/internal/bandwidth_upgrade_manager.h"
#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel_manager.h"
#include "core/internal/endpoint_manager.h"
#include "core/internal/medium_manager.h"
#include "core/internal/payload_manager.h"
#include "core/internal/pcp_manager.h"
#include "core/internal/service_controller.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/payload.h"
#include "core/status.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class OfflineServiceController : public ServiceController<Platform> {
 public:
  OfflineServiceController();
  ~OfflineServiceController() override;

  Status::Value startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
      const string& service_id, const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) override;
  void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy) override;

  Status::Value startDiscovery(
      Ptr<ClientProxy<Platform> > client_proxy, const string& service_id,
      const DiscoveryOptions& discovery_options,
      Ptr<DiscoveryListener> discovery_listener) override;
  void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy) override;

  Status::Value requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_name,
      const string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) override;
  Status::Value acceptConnection(
      Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
      Ptr<PayloadListener> payload_listener) override;
  Status::Value rejectConnection(Ptr<ClientProxy<Platform> > client_proxy,
                                 const string& endpoint_id) override;

  void initiateBandwidthUpgrade(Ptr<ClientProxy<Platform> > client_proxy,
                                const string& endpoint_id) override;

  void sendPayload(Ptr<ClientProxy<Platform> > client_proxy,
                   const std::vector<string>& endpoint_ids,
                   ConstPtr<Payload> payload) override;
  Status::Value cancelPayload(Ptr<ClientProxy<Platform> > client_proxy,
                              std::int64_t payload_id) override;

  void disconnectFromEndpoint(Ptr<ClientProxy<Platform> > client_proxy,
                              const string& endpoint_id) override;

 private:
  // Note that the order of declaration of these is crucial, because we depend
  // on the destructors running (strictly) in the reverse order; a deviation
  // from that will lead to crashes at runtime.
  ScopedPtr<Ptr<MediumManager<Platform> > > medium_manager_;
  ScopedPtr<Ptr<EndpointChannelManager>> endpoint_channel_manager_;
  ScopedPtr<Ptr<EndpointManager<Platform> > > endpoint_manager_;
  ScopedPtr<Ptr<PayloadManager<Platform> > > payload_manager_;
  ScopedPtr<Ptr<BandwidthUpgradeManager>> bandwidth_upgrade_manager_;
  ScopedPtr<Ptr<PCPManager<Platform> > > pcp_manager_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/offline_service_controller.cc"

#endif  // CORE_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_
