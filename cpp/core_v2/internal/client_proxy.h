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

#ifndef CORE_V2_INTERNAL_CLIENT_PROXY_H_
#define CORE_V2_INTERNAL_CLIENT_PROXY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/status.h"
#include "core_v2/strategy.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/prng.h"
#include "platform_v2/public/mutex.h"
#include "proto/connections_enums.pb.h"
// Prefer using absl:: versions of a set and a map; they tend to be more
// efficient: implementation is using open-addressing hash tables.
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {

// CLientProxy is tracking state of client's connection, and serves as
// a proxy for notifications sent to this client.
class ClientProxy final {
 public:
  static constexpr int kEndpointIdLength = 4;

  ClientProxy();
  ~ClientProxy();
  ClientProxy(ClientProxy&&) = default;
  ClientProxy& operator=(ClientProxy&&) = default;

  std::int64_t GetClientId() const;

  std::string GetLocalEndpointId();

  // Clears all the runtime state of this client.
  void Reset();

  // Marks this client as advertising with the given callbacks.
  void StartedAdvertising(
      const std::string& service_id, Strategy strategy,
      const ConnectionListener& connection_lifecycle_listener,
      absl::Span<proto::connections::Medium> mediums);
  // Marks this client as not advertising.
  void StoppedAdvertising();
  bool IsAdvertising() const;
  std::string GetAdvertisingServiceId() const;

  // Get service ID of a surrently active link (either advertising, or
  // discovering).
  std::string GetServiceId() const;

  // Marks this client as discovering with the given callback.
  void StartedDiscovery(const std::string& service_id, Strategy strategy,
                        const DiscoveryListener& discovery_listener,
                        absl::Span<proto::connections::Medium> mediums);
  // Marks this client as not discovering at all.
  void StoppedDiscovery();
  bool IsDiscoveringServiceId(const std::string& service_id) const;
  bool IsDiscovering() const;
  std::string GetDiscoveryServiceId() const;

  // Proxies to the client's DiscoveryListener::OnEndpointFound() callback.
  void OnEndpointFound(const std::string& service_id,
                       const std::string& endpoint_id,
                       const ByteArray& endpoint_info,
                       proto::connections::Medium medium);
  // Proxies to the client's DiscoveryListener::OnEndpointLost() callback.
  void OnEndpointLost(const std::string& service_id,
                      const std::string& endpoint_id);

  // Proxies to the client's ConnectionListener::OnInitiated() callback.
  void OnConnectionInitiated(const std::string& endpoint_id,
                             const ConnectionResponseInfo& info,
                             const ConnectionOptions& options,
                             const ConnectionListener& listener);

  // Proxies to the client's ConnectionListener::OnAccepted() callback.
  void OnConnectionAccepted(const std::string& endpoint_id);
  // Proxies to the client's ConnectionListener::OnRejected() callback.
  void OnConnectionRejected(const std::string& endpoint_id,
                            const Status& status);

  void OnBandwidthChanged(const std::string& endpoint_id, Medium new_medium);

  // Removes the endpoint from this client's list of connected endpoints. If
  // notify is true, also calls the client's
  // ConnectionListener.disconnected_cb() callback.
  void OnDisconnected(const std::string& endpoint_id, bool notify);

  // Returns all mediums eligible for upgrade.
  BooleanMediumSelector GetUpgradeMediums(const std::string& endpoint_id) const;
  // Returns true if it's safe to send payloads to this endpoint.
  bool IsConnectedToEndpoint(const std::string& endpoint_id) const;
  // Returns all endpoints that can safely be sent payloads.
  std::vector<std::string> GetConnectedEndpoints() const;
  // Returns all endpoints that are still awaiting acceptance.
  std::vector<std::string> GetPendingConnectedEndpoints() const;
  // Returns the number of endpoints that are connected and outgoing.
  std::int32_t GetNumOutgoingConnections() const;
  // Returns the number of endpoints that are connected and incoming.
  std::int32_t GetNumIncomingConnections() const;
  // If true, then we're in the process of approving (or rejecting) a
  // connection. No payloads should be sent until isConnectedToEndpoint()
  // returns true.
  bool HasPendingConnectionToEndpoint(const std::string& endpoint_id) const;
  // Returns true if the local endpoint has already marked itself as
  // accepted/rejected.
  bool HasLocalEndpointResponded(const std::string& endpoint_id) const;
  // Returns true if the remote endpoint has already marked themselves as
  // accepted/rejected.
  bool HasRemoteEndpointResponded(const std::string& endpoint_id) const;
  // Marks the local endpoint as having accepted the connection.
  void LocalEndpointAcceptedConnection(const std::string& endpoint_id,
                                       const PayloadListener& listener);
  // Marks the local endpoint as having rejected the connection.
  void LocalEndpointRejectedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having accepted the connection.
  void RemoteEndpointAcceptedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having rejected the connection.
  void RemoteEndpointRejectedConnection(const std::string& endpoint_id);
  // Returns true if both the local endpoint and the remote endpoint have
  // accepted the connection.
  bool IsConnectionAccepted(const std::string& endpoint_id) const;
  // Returns true if either the local endpoint or the remote endpoint has
  // rejected the connection.
  bool IsConnectionRejected(const std::string& endpoint_id) const;

  // Proxies to the client's PayloadListener::OnPayload() callback.
  void OnPayload(const std::string& endpoint_id, Payload payload);
  // Proxies to the client's PayloadListener::OnPayloadProgress() callback.
  void OnPayloadProgress(const std::string& endpoint_id,
                         const PayloadProgressInfo& info);
  bool LocalConnectionIsAccepted(std::string endpoint_id) const;
  bool RemoteConnectionIsAccepted(std::string endpoint_id) const;

 private:
  struct Connection {
    // Status: may be either:
    // Connection::PENDING, or combination of
    // Connection::LOCAL_ENDPOINT_ACCEPTED:
    // Connection::LOCAL_ENDPOINT_REJECTED and
    // Connection::REMOTE_ENDPOINT_ACCEPTED:
    // Connection::REMOTE_ENDPOINT_REJECTED, or
    // Connection::CONNECTED.
    // Only when this is set to CONNECTED should you allow payload transfers.
    //
    // We want this enum to be implicitly convertible to int, because
    // we perform bit operations on it.
    enum Status : uint8_t {
      kPending = 0,
      kLocalEndpointAccepted = 1 << 0,
      kLocalEndpointRejected = 1 << 1,
      kRemoteEndpointAccepted = 1 << 2,
      kRemoteEndpointRejected = 1 << 3,
      kConnected = 1 << 4,
    };
    bool is_incoming{false};
    Status status{kPending};
    ConnectionListener connection_listener;
    PayloadListener payload_listener;
    ConnectionOptions connection_options;
  };

  struct AdvertisingInfo {
    std::string service_id;
    ConnectionListener listener;
    void Clear() { service_id.clear(); }
    bool IsEmpty() const { return service_id.empty(); }
  };

  struct DiscoveryInfo {
    std::string service_id;
    DiscoveryListener listener;
    void Clear() { service_id.clear(); }
    bool IsEmpty() const { return service_id.empty(); }
  };

  void RemoveAllEndpoints();
  bool ConnectionStatusesContains(const std::string& endpoint_id,
                                  Connection::Status status_to_match) const;
  void AppendConnectionStatus(const std::string& endpoint_id,
                              Connection::Status status_to_append);

  const Connection* LookupConnection(const std::string& endpoint_id) const;
  Connection* LookupConnection(const std::string& endpoint_id);
  bool ConnectionStatusMatches(const std::string& endpoint_id,
                               Connection::Status status) const;
  std::vector<std::string> GetMatchingEndpoints(
      std::function<bool(const Connection&)> pred) const;

  mutable RecursiveMutex mutex_;
  std::int64_t client_id_;
  std::string local_endpoint_id_;
  Prng prng_;

  // If not empty, we are currently advertising and accepting connection
  // requests for the given service_id.
  AdvertisingInfo advertising_info_;

  // If not empty, we are currently discovering for the given service_id.
  DiscoveryInfo discovery_info_;

  // Maps endpoint_id to endpoint connection state.
  absl::flat_hash_map<std::string, Connection> connections_;

  // A cache of endpoint ids that we've already notified the discoverer of. We
  // check this cache before calling onEndpointFound() so that we don't notify
  // the client multiple times for the same endpoint. This would otherwise
  // happen because some mediums (like Bluetooth) repeatedly give us the same
  // endpoints after each scan.
  absl::flat_hash_set<std::string> discovered_endpoint_ids_;
};

// Operator overloads when comparing Ptr<ClientProxy>.
bool operator==(const ClientProxy& lhs, const ClientProxy& rhs);
bool operator<(const ClientProxy& lhs, const ClientProxy& rhs);

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_CLIENT_PROXY_H_
