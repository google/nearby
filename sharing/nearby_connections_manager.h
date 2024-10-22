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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {

class NearbyConnection;
class PayloadTransferUpdatePtr;

using ConnectionsStatus = nearby::sharing::Status;

class NearbyConnectionsManager {
 public:
  using ConnectionsCallback = std::function<void(ConnectionsStatus status)>;
  using NearbyConnectionCallback =
      std::function<void(NearbyConnection*, Status status)>;

  // A callback for handling incoming connections while advertising.
  class IncomingConnectionListener {
   public:
    virtual ~IncomingConnectionListener() = default;

    // `endpoint_info`is returned from remote devices and should be parsed in
    // utility process.
    virtual void OnIncomingConnection(absl::string_view endpoint_id,
                                      absl::Span<const uint8_t> endpoint_info,
                                      NearbyConnection* connection) = 0;
  };

  // A callback for handling discovered devices while discovering.
  class DiscoveryListener {
   public:
    virtual ~DiscoveryListener() = default;

    // `endpoint_info` is returned from remote devices and should be parsed in
    // utility process.
    virtual void OnEndpointDiscovered(
        absl::string_view endpoint_id,
        absl::Span<const uint8_t> endpoint_info) = 0;

    // A callback triggered when the endpoint is lost.
    virtual void OnEndpointLost(absl::string_view endpoint_id) = 0;
  };

  // A callback for tracking the status of a payload (both incoming and
  // outgoing).
  class PayloadStatusListener
      : public std::enable_shared_from_this<PayloadStatusListener> {
   public:
    PayloadStatusListener() = default;
    virtual ~PayloadStatusListener() = default;

    std::weak_ptr<PayloadStatusListener> GetWeakPtr() {
      return weak_from_this();
    }

    // Note: `upgraded_medium` is passed in for use in metrics, and it is
    // absl::nullopt if the bandwidth has not upgraded yet or if the upgrade
    // status is not known.
    virtual void OnStatusUpdate(std::unique_ptr<PayloadTransferUpdate> update,
                                std::optional<Medium> upgraded_medium) = 0;
  };

  // Converts the status to a logging-friendly string.
  static std::string ConnectionsStatusToString(ConnectionsStatus status);

  NearbyConnectionsManager() = default;
  virtual ~NearbyConnectionsManager() = default;

  // Disconnects from all endpoints and shut down Nearby Connections.
  // As a side effect of this call, both StopAdvertising and StopDiscovery may
  // be invoked if Nearby Connections is advertising or discovering.
  virtual void Shutdown() = 0;

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // `listener` remains valid until StopAdvertising is called.
  virtual void StartAdvertising(std::vector<uint8_t> endpoint_info,
                                IncomingConnectionListener* listener,
                                PowerLevel power_level,
                                proto::DataUsage data_usage,
                                bool use_stable_endpoint_id,
                                ConnectionsCallback callback) = 0;

  // Stops advertising through Nearby Connections.
  virtual void StopAdvertising(ConnectionsCallback callback) = 0;

  // Starts discovery through Nearby Connections. Caller is expected to ensure
  // `listener` remains valid until StopDiscovery is called.
  virtual void StartDiscovery(DiscoveryListener* listener,
                              proto::DataUsage data_usage,
                              ConnectionsCallback callback) = 0;

  // Stops discovery through Nearby Connections.
  virtual void StopDiscovery() = 0;

  // Connects to remote `endpoint_id` through Nearby Connections.
  virtual void Connect(
      std::vector<uint8_t> endpoint_info, absl::string_view endpoint_id,
      std::optional<std::vector<uint8_t>> bluetooth_mac_address,
      proto::DataUsage data_usage, TransportType transport_type,
      NearbyConnectionCallback callback) = 0;

  // Disconnects from remote `endpoint_id` through Nearby Connections.
  virtual void Disconnect(absl::string_view endpoint_id) = 0;

  // Sends `payload` through Nearby Connections.
  virtual void Send(absl::string_view endpoint_id,
                    std::unique_ptr<Payload> payload,
                    std::weak_ptr<PayloadStatusListener> listener) = 0;

  // Register a `listener` with `payload_id`.
  virtual void RegisterPayloadStatusListener(
      int64_t payload_id, std::weak_ptr<PayloadStatusListener> listener) = 0;

  // Gets the payload associated with `payload_id` if available.
  virtual const Payload* GetIncomingPayload(int64_t payload_id) const = 0;

  // Cancels a Payload currently in-flight to or from remote endpoints.
  virtual void Cancel(int64_t payload_id) = 0;

  // Clears all incoming payloads.
  virtual void ClearIncomingPayloads() = 0;

  // Gets the raw authentication token for the `endpoint_id`.
  virtual std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      absl::string_view endpoint_id) = 0;

  // Initiates bandwidth upgrade for `endpoint_id`.
  virtual void UpgradeBandwidth(absl::string_view endpoint_id) = 0;

  // Sets a custom save path.
  virtual void SetCustomSavePath(absl::string_view custom_save_path) = 0;

  // Gets the file paths to delete and clear the hash set.
  virtual absl::flat_hash_set<std::filesystem::path>
  GetAndClearUnknownFilePathsToDelete() = 0;

  // Dump internal state for debugging purposes.
  virtual std::string Dump() const = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_H_
