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

#ifndef CORE_INTERNAL_PCP_HANDLER_H_
#define CORE_INTERNAL_PCP_HANDLER_H_

#include <utility>
#include <vector>

#include "connections/implementation/client_proxy.h"
#include "connections/implementation/pcp.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/interop/device.h"

namespace nearby {
namespace connections {

inline Pcp StrategyToPcp(Strategy strategy) {
  if (strategy == Strategy::kP2pCluster) return Pcp::kP2pCluster;
  if (strategy == Strategy::kP2pStar) return Pcp::kP2pStar;
  if (strategy == Strategy::kP2pPointToPoint) return Pcp::kP2pPointToPoint;
  return Pcp::kUnknown;
}

inline Strategy PcpToStrategy(Pcp pcp) {
  if (pcp == Pcp::kP2pCluster) return Strategy::kP2pCluster;
  if (pcp == Pcp::kP2pStar) return Strategy::kP2pStar;
  if (pcp == Pcp::kP2pPointToPoint) return Strategy::kP2pPointToPoint;
  return Strategy::kNone;
}

// Defines the set of methods that need to be implemented to handle the
// per-PCP-specific operations in the OfflineServiceController.
//
// These methods are all meant to be synchronous, and should return only after
// knowing they've done what they were supposed to do (or unequivocally failed
// to do so).
//
// See details here:
// cpp/core/core.h
class PcpHandler {
 public:
  virtual ~PcpHandler() = default;

  // Return strategy supported by this protocol.
  virtual Strategy GetStrategy() const = 0;

  // Return concrete variant of protocol.
  virtual Pcp GetPcp() const = 0;

  // We have been asked by the client to start advertising. Once we successfully
  // start advertising, we'll change the ClientProxy's state.
  // ConnectionListener (info.listener) will be notified in case of any event.
  // See
  // cpp/core/listeners.h
  virtual Status StartAdvertising(ClientProxy* client,
                                  const std::string& service_id,
                                  const AdvertisingOptions& advertising_options,
                                  const ConnectionRequestInfo& info) = 0;

  // If Advertising is active, stop it, and change CLientProxy state,
  // otherwise do nothing.
  virtual void StopAdvertising(ClientProxy* client) = 0;

  // Start discovery of endpoints that may be advertising.
  // Update ClientProxy state once discovery started.
  // DiscoveryListener will get called in case of any event.
  virtual Status StartDiscovery(ClientProxy* client,
                                const std::string& service_id,
                                const DiscoveryOptions& discovery_options,
                                DiscoveryListener listener) = 0;

  // If Discovery is active, stop it, and change CLientProxy state,
  // otherwise do nothing.
  virtual void StopDiscovery(ClientProxy* client) = 0;

  virtual std::pair<Status, std::vector<ConnectionInfoVariant>>
  StartListeningForIncomingConnections(
      ClientProxy* client, absl::string_view service_id,
      v3::ConnectionListeningOptions options,
      v3::ConnectionListener connection_listener) = 0;

  virtual void StopListeningForIncomingConnections(ClientProxy* client) = 0;

  // If Discovery is active with is_out_of_band_connection == true, invoke the
  // callback with the provided endpoint info.
  virtual void InjectEndpoint(ClientProxy* client,
                              const std::string& service_id,
                              const OutOfBandConnectionMetadata& metadata) = 0;

  // If remote endpoint has been successfully discovered, request it to form a
  // connection, update state on ClientProxy.
  virtual Status RequestConnection(
      ClientProxy* client, const std::string& endpoint_id,
      const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options) = 0;

  virtual Status RequestConnectionV3(
      ClientProxy* client, const NearbyDevice& remote_device,
      const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options) = 0;

  // Either party may call this to accept connection on their part.
  // Until both parties call it, connection will not reach a data phase.
  // Update state in ClientProxy.
  virtual Status AcceptConnection(ClientProxy* client,
                                  const std::string& endpoint_id,
                                  PayloadListener payload_listener) = 0;

  // Either party may call this to reject connection on their part before
  // connection reaches data phase. If either party does call it, connection
  // will terminate. Update state in ClientProxy.
  virtual Status RejectConnection(ClientProxy* client,
                                  const std::string& endpoint_id) = 0;

  virtual Status UpdateAdvertisingOptions(
      ClientProxy* client, absl::string_view service_id,
      const AdvertisingOptions& advertising_options) = 0;

  virtual Status UpdateDiscoveryOptions(
      ClientProxy* client, absl::string_view service_id,
      const DiscoveryOptions& discovery_options) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_PCP_HANDLER_H_
