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

#ifndef CORE_INTERNAL_PCP_MANAGER_H_
#define CORE_INTERNAL_PCP_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/pcp_handler.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/listeners.h"
#include "internal/interop/device.h"
#include "internal/platform/atomic_boolean.h"

namespace nearby {
namespace connections {

// Manages all known PcpHandler implementations, delegating operations to the
// appropriate one as per the parameters passed in.
//
// This will only ever be used by the OfflineServiceController, which has all
// of its entrypoints invoked serially, so there's no synchronization needed.
// Public method semantics matches definition in the
// cpp/core/internal/service_controller.h
class PcpManager {
 public:
  PcpManager(Mediums& mediums, EndpointChannelManager& channel_manager,
             EndpointManager& endpoint_manager, BwuManager& bwu_manager,
             InjectedBluetoothDeviceStore& injected_bluetooth_device_store);
  ~PcpManager();

  Status StartAdvertising(ClientProxy* client, const std::string& service_id,
                          const AdvertisingOptions& advertising_options,
                          const ConnectionRequestInfo& info);
  void StopAdvertising(ClientProxy* client);

  Status StartDiscovery(ClientProxy* client, const std::string& service_id,
                        const DiscoveryOptions& discovery_options,
                        DiscoveryListener listener);
  void StopDiscovery(ClientProxy* client);

  std::pair<Status, std::vector<ConnectionInfoVariant>>
  StartListeningForIncomingConnections(
      ClientProxy* client, absl::string_view service_id,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options);

  void StopListeningForIncomingConnections(ClientProxy* client);

  void InjectEndpoint(ClientProxy* client, const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata);

  Status RequestConnection(ClientProxy* client, const std::string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& connection_options);

  Status RequestConnectionV3(ClientProxy* client,
                             const NearbyDevice& remote_device,
                             const ConnectionRequestInfo& info,
                             const ConnectionOptions& connection_options);
  Status AcceptConnection(ClientProxy* client, const std::string& endpoint_id,
                          PayloadListener payload_listener);
  Status RejectConnection(ClientProxy* client, const std::string& endpoint_id);

  Status UpdateAdvertisingOptions(
      ClientProxy* client, absl::string_view service_id,
      const AdvertisingOptions& advertising_options);

  Status UpdateDiscoveryOptions(ClientProxy* client,
                                absl::string_view service_id,
                                const DiscoveryOptions& discovery_options);

  location::nearby::proto::connections::Medium GetBandwidthUpgradeMedium();
  void DisconnectFromEndpointManager();

 private:
  bool SetCurrentPcpHandler(Strategy strategy);
  PcpHandler* GetPcpHandler(Pcp pcp) const;
  bool GetWebRtcNonCellular(const std::vector<Medium>& mediums);

  AtomicBoolean shutdown_{false};
  absl::flat_hash_map<Pcp, std::unique_ptr<BasePcpHandler>> handlers_;
  PcpHandler* current_ = nullptr;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_PCP_MANAGER_H_
