// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/platform/listeners.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// Main interface to control the NearbyConnections library. Implemented in a
// sandboxed process. This interface is used by the browser process to connect
// to remote devices and send / receive raw data packets. Parsing of those
// packets is not part of the NearbyConnections library and is done in a
// separate interface.
class NearbyConnectionsService {
 public:
  virtual ~NearbyConnectionsService() = default;

  struct ConnectionListener {
    std::function<void(const std::string& endpoint_id,
                       const ConnectionInfo& info)>
        initiated_cb =
            DefaultFuncCallback<const std::string&, const ConnectionInfo&>();

    std::function<void(const std::string& endpoint_id)> accepted_cb =
        DefaultFuncCallback<const std::string&>();

    std::function<void(const std::string& endpoint_id, Status status)>
        rejected_cb = DefaultFuncCallback<const std::string&, Status>();

    std::function<void(const std::string& endpoint_id)> disconnected_cb =
        DefaultFuncCallback<const std::string&>();

    std::function<void(const std::string& endpoint_id, Medium medium)>
        bandwidth_changed_cb =
            DefaultFuncCallback<const std::string&, Medium>();
  };

  struct DiscoveryListener {
    std::function<void(const std::string& endpoint_id,
                       const DiscoveredEndpointInfo& info)>
        endpoint_found_cb =
            DefaultFuncCallback<const std::string&,
                                const DiscoveredEndpointInfo&>();

    std::function<void(const std::string& endpoint_id)> endpoint_lost_cb =
        DefaultFuncCallback<const std::string&>();

    std::function<void(const std::string& endpoint_id, DistanceInfo info)>
        endpoint_distance_changed_cb =
            DefaultFuncCallback<const std::string&, DistanceInfo>();
  };

  struct PayloadListener {
    std::function<void(absl::string_view endpoint_id, Payload payload)>
        payload_cb = DefaultFuncCallback<absl::string_view, Payload>();

    std::function<void(absl::string_view endpoint_id,
                       const PayloadTransferUpdate& update)>
        payload_progress_cb =
            DefaultFuncCallback<absl::string_view,
                                const PayloadTransferUpdate&>();
  };

  virtual void StartAdvertising(
      absl::string_view service_id, const std::vector<uint8_t>& endpoint_info,
      const AdvertisingOptions& advertising_options,
      ConnectionListener advertising_listener,
      std::function<void(Status status)> callback) = 0;
  virtual void StopAdvertising(absl::string_view service_id,
                               std::function<void(Status status)> callback) = 0;

  virtual void StartDiscovery(absl::string_view service_id,
                              const DiscoveryOptions& discovery_options,
                              DiscoveryListener discovery_listener,
                              std::function<void(Status status)> callback) = 0;
  virtual void StopDiscovery(absl::string_view service_id,
                             std::function<void(Status status)> callback) = 0;

  virtual void RequestConnection(
      absl::string_view service_id, const std::vector<uint8_t>& endpoint_info,
      absl::string_view endpoint_id,
      const ConnectionOptions& connection_options,
      ConnectionListener connection_listener,
      std::function<void(Status status)> callback) = 0;

  virtual void DisconnectFromEndpoint(
      absl::string_view service_id, absl::string_view endpoint_id,
      std::function<void(Status status)> callback) = 0;

  virtual void SendPayload(absl::string_view service_id,
                           absl::Span<const std::string> endpoint_ids,
                           std::unique_ptr<Payload> payload,
                           std::function<void(Status status)> callback) = 0;
  virtual void CancelPayload(absl::string_view service_id, int64_t payload_id,
                             std::function<void(Status status)> callback) = 0;

  virtual void InitiateBandwidthUpgrade(
      absl::string_view service_id, absl::string_view endpoint_id,
      std::function<void(Status status)> callback) = 0;

  virtual void AcceptConnection(
      absl::string_view service_id, absl::string_view endpoint_id,
      PayloadListener payload_listener,
      std::function<void(Status status)> callback) = 0;

  virtual void StopAllEndpoints(
      std::function<void(Status status)> callback) = 0;

  virtual void SetCustomSavePath(
      absl::string_view path, std::function<void(Status status)> callback) = 0;

  virtual std::string Dump() const = 0;
};

Status ConvertToStatus(nearby::connections::Status status);
Payload ConvertToPayload(nearby::connections::Payload payload);
nearby::connections::Payload ConvertToServicePayload(Payload payload);
nearby::connections::ResultCallback BuildResultCallback(
    std::function<void(Status status)> callback);
nearby::connections::Strategy ConvertToServiceStrategy(Strategy strategy);

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_SERVICE_H_
