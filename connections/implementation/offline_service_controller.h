// Copyright 2021 Google LLC
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
#include <string>
#include <vector>

#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/payload_manager.h"
#include "connections/implementation/pcp_manager.h"
#include "connections/implementation/service_controller.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/platform/borrowable.h"

namespace nearby {
namespace connections {

class OfflineServiceController : public ServiceController {
 public:
  OfflineServiceController() = default;
  ~OfflineServiceController() override;

  Status StartAdvertising(::nearby::Borrowable<ClientProxy*> client,
                          const std::string& service_id,
                          const AdvertisingOptions& advertising_options,
                          const ConnectionRequestInfo& info) override;

  void StopAdvertising(::nearby::Borrowable<ClientProxy*> client) override;

  Status StartDiscovery(::nearby::Borrowable<ClientProxy*> client,
                        const std::string& service_id,
                        const DiscoveryOptions& discovery_options,
                        const DiscoveryListener& listener) override;
  void StopDiscovery(::nearby::Borrowable<ClientProxy*> client) override;

  void InjectEndpoint(::nearby::Borrowable<ClientProxy*> client,
                      const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata) override;

  std::pair<Status, std::vector<ConnectionInfoVariant>>
  StartListeningForIncomingConnections(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options) override;

  void StopListeningForIncomingConnections(
      ::nearby::Borrowable<ClientProxy*> client) override;

  Status RequestConnection(
      ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id,
      const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options) override;
  Status AcceptConnection(::nearby::Borrowable<ClientProxy*> client,
                          const std::string& endpoint_id,
                          PayloadListener listener) override;
  Status RejectConnection(::nearby::Borrowable<ClientProxy*> client,
                          const std::string& endpoint_id) override;

  void InitiateBandwidthUpgrade(::nearby::Borrowable<ClientProxy*> client,
                                const std::string& endpoint_id) override;

  void SendPayload(::nearby::Borrowable<ClientProxy*> client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload) override;
  Status CancelPayload(::nearby::Borrowable<ClientProxy*> client,
                       Payload::Id payload_id) override;

  void DisconnectFromEndpoint(::nearby::Borrowable<ClientProxy*> client,
                              const std::string& endpoint_id) override;

  Status UpdateAdvertisingOptions(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const AdvertisingOptions& advertising_options) override;

  Status UpdateDiscoveryOptions(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const DiscoveryOptions& discovery_options) override;

  void Stop() override;

  void SetCustomSavePath(::nearby::Borrowable<ClientProxy*> client,
                         const std::string& path) override;

  void ShutdownBwuManagerExecutors() override;

 private:
  // Note that the order of declaration of these is crucial, because we depend
  // on the destructors running (strictly) in the reverse order; a deviation
  // from that will lead to crashes at runtime.
  AtomicBoolean stop_{false};
  Mediums mediums_;
  EndpointChannelManager channel_manager_;
  EndpointManager endpoint_manager_{&channel_manager_};
  PayloadManager payload_manager_{endpoint_manager_};
  BwuManager bwu_manager_{
      mediums_, endpoint_manager_, channel_manager_, {}, {}};
  InjectedBluetoothDeviceStore injected_bluetooth_device_store_;
  PcpManager pcp_manager_{mediums_, channel_manager_, endpoint_manager_,
                          bwu_manager_, injected_bluetooth_device_store_};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_OFFLINE_SERVICE_CONTROLLER_H_
