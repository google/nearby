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

#ifndef CORE_INTERNAL_CLIENT_PROXY_H_
#define CORE_INTERNAL_CLIENT_PROXY_H_

#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "core/listeners.h"
#include "core/strategy.h"
#include "platform/api/lock.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class ClientProxy {
 public:
  static const std::int32_t kEndpointIdLength;

  ClientProxy();
  ~ClientProxy();

  std::int64_t getClientId() const;

  std::string generateLocalEndpointId();

  // Clears all the runtime state of this client.
  void reset();

  // Marks this client as advertising with the given callbacks.
  void startedAdvertising(
      const std::string& service_id, const Strategy& strategy,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
      const std::vector<proto::connections::Medium>& mediums);
  // Marks this client as not advertising.
  void stoppedAdvertising();
  bool isAdvertising();
  std::string getAdvertisingServiceId();

  // Marks this client as discovering with the given callback.
  void startedDiscovery(const std::string& service_id, const Strategy& strategy,
                        Ptr<DiscoveryListener> discovery_listener,
                        const std::vector<proto::connections::Medium>& mediums);
  // Marks this client as not discovering at all.
  void stoppedDiscovery();
  bool isDiscoveringServiceId(const std::string& service_id);
  bool isDiscovering();
  std::string getDiscoveryServiceId();

  // Proxies to the client's DiscoveryListener.onEndpointFound() callback.
  void onEndpointFound(const std::string& endpoint_id,
                       const std::string& service_id,
                       const std::string& endpoint_name,
                       proto::connections::Medium medium);
  // Proxies to the client's DiscoveryListener.onEndpointLost() callback.
  void onEndpointLost(const std::string& service_id,
                      const std::string& endpoint_id);

  // Proxies to the client's ConnectionLifecycleListener.onConnectionInitiated()
  // callback.
  void onConnectionInitiated(
      const std::string& endpoint_id, const std::string& endpoint_name,
      const std::string& authentication_token,
      ConstPtr<ByteArray> raw_authentication_token, bool is_incoming_connection,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener);
  // Proxies to the client's ConnectionLifecycleListener.onConnectionResult()
  // callback.
  void onConnectionResult(const std::string& endpoint_id, Status::Value status);

  void onBandwidthChanged(const std::string& endpoint_id, std::int32_t quality);

  // Removes the endpoint from this client's list of connected endpoints. If
  // notify is true, also calls the client's
  // ConnectionLifecycleListener.onDisconnected() callback.
  void onDisconnected(const std::string& endpoint_id, bool notify);

  // Returns true if it's safe to send payloads to this endpoint.
  bool isConnectedToEndpoint(const std::string& endpoint_id);
  // Returns all endpoints that can safely be sent payloads.
  std::vector<std::string> getConnectedEndpoints();
  // Returns all endpoints that are still awaiting acceptance.
  std::vector<std::string> getPendingConnectedEndpoints();
  // Returns the number of endpoints that are connected and outgoing.
  std::int32_t getNumOutgoingConnections();
  // Returns the number of endpoints that are connected and incoming.
  std::int32_t getNumIncomingConnections();
  // If true, then we're in the process of approving (or rejecting) a
  // connection. No payloads should be sent until isConnectedToEndpoint()
  // returns true.
  bool hasPendingConnectionToEndpoint(const std::string& endpoint_id);
  // Returns true if the local endpoint has already marked itself as
  // accepted/rejected.
  bool hasLocalEndpointResponded(const std::string& endpoint_id);
  // Returns true if the remote endpoint has already marked themselves as
  // accepted/rejected.
  bool hasRemoteEndpointResponded(const std::string& endpoint_id);
  // Marks the local endpoint as having accepted the connection.
  void localEndpointAcceptedConnection(const std::string& endpoint_id,
                                       Ptr<PayloadListener> payload_listener);
  // Marks the local endpoint as having rejected the connection.
  void localEndpointRejectedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having accepted the connection.
  void remoteEndpointAcceptedConnection(const std::string& endpoint_id);
  // Marks the remote endpoint as having rejected the connection.
  void remoteEndpointRejectedConnection(const std::string& endpoint_id);
  // Returns true if both the local endpoint and the remote endpoint have
  // accepted the connection.
  bool isConnectionAccepted(const std::string& endpoint_id);
  // Returns true if either the local endpoint or the remote endpoint has
  // rejected the connection.
  bool isConnectionRejected(const std::string& endpoint_id);

  // Proxies to the client's PayloadListener.onPayloadReceived() callback.
  void onPayloadReceived(const std::string& endpoint_id,
                         ConstPtr<Payload> payload);
  // Proxies to the client's PayloadListener.onPayloadTransferUpdate() callback.
  void onPayloadTransferUpdate(
      const std::string& endpoint_id,
      const PayloadTransferUpdate& payload_transfer_update);

  // Operator overloads when comparing Ptr<ClientProxy>.
  bool operator==(const ClientProxy<Platform>& rhs);
  bool operator<(const ClientProxy<Platform>& rhs);

 private:
  struct ConnectionEstablishmentStatus {
    enum Value {
      PENDING = 0,
      LOCAL_ENDPOINT_ACCEPTED = 1 << 0,
      LOCAL_ENDPOINT_REJECTED = 1 << 1,
      REMOTE_ENDPOINT_ACCEPTED = 1 << 2,
      REMOTE_ENDPOINT_REJECTED = 1 << 3,
      CONNECTED = 1 << 4,
    };
  };

  struct AdvertisingInfo {
    const std::string service_id;
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener;

    AdvertisingInfo(
        const std::string& service_id,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener)
        : service_id(service_id),
          connection_lifecycle_listener(connection_lifecycle_listener) {}
  };

  struct DiscoveryInfo {
    const std::string service_id;
    ScopedPtr<Ptr<DiscoveryListener> > discovery_listener;

    DiscoveryInfo(const std::string& service_id,
                  Ptr<DiscoveryListener> discovery_listener)
        : service_id(service_id), discovery_listener(discovery_listener) {}
  };

  struct ConnectionMetadata {
    const bool is_incoming;
    typename ConnectionEstablishmentStatus::Value status;

    explicit ConnectionMetadata(bool is_incoming)
        : is_incoming(is_incoming),
          status(ConnectionEstablishmentStatus::PENDING) {}
  };

  void removeAllEndpoints();

  bool connectionEstablishmentStatusesContains(
      const std::string& endpoint_id,
      typename ConnectionEstablishmentStatus::Value status_to_match);
  void appendConnectionEstablishmentStatus(
      const std::string& endpoint_id,
      typename ConnectionEstablishmentStatus::Value status_to_append);

  ScopedPtr<Ptr<Lock> > lock_;
  const std::int64_t client_id_;

  // If set, we are currently advertising and accepting connection requests for
  // the given service_id.
  Ptr<AdvertisingInfo> advertising_info_;

  // If set, we are currently discovering for the given service_id.
  Ptr<DiscoveryInfo> discovery_info_;

  /**
   * Map of endpoint_ids -> ConnectionMetadata. ConnectionMetadata.status may be
   * either ConnectionEstablishmentStatus::PENDING, a combination of
   * ConnectionEstablishmentStatus::LOCAL_ENDPOINT_ACCEPTED:
   * ConnectionEstablishmentStatus::LOCAL_ENDPOINT_REJECTED and
   * ConnectionEstablishmentStatus::REMOTE_ENDPOINT_ACCEPTED:
   * ConnectionEstablishmentStatus::REMOTE_ENDPOINT_REJECTED, or
   * ConnectionEstablishmentStatus::CONNECTED. Only when this is set to
   * CONNECTED should you allow payload transfers.
   */
  typedef std::map<std::string, ConnectionMetadata>
      ConnectionEstablishmentStatusesMap;
  ConnectionEstablishmentStatusesMap connection_establishment_statuses_;

  /**
   * Map of endpoint_ids -> ConnectionLifecycleListeners. Every endpoint in here
   * is guaranteed to at least be in
   * ConnectionEstablishmentStatus::PENDING -- the precise status can be found
   * from the corresponding entry in connection_establishment_statuses.
   */
  typedef std::map<std::string, Ptr<ConnectionLifecycleListener> >
      ConnectionLifecycleListenersMap;
  ConnectionLifecycleListenersMap connection_lifecycle_listeners_;

  /**
   * Map of endpoint_ids -> PayloadListeners. Every endpoint in here is
   * guaranteed to at least be in
   * ConnectionEstablishmentStatus::LOCAL_ENDPOINT_ACCEPTED -- the
   * precise status can be found from the corresponding entry in
   * connection_establishment_statuses.
   */
  typedef std::map<std::string, Ptr<PayloadListener> > PayloadListenersMap;
  PayloadListenersMap payload_listeners_;

  /**
   * A cache of endpoint ids that we've already notified the discoverer of. We
   * check this cache before calling onEndpointFound() so that we don't notify
   * the client multiple times for the same endpoint. This would otherwise
   * happen because some mediums (like Bluetooth) repeatedly give us the same
   * endpoints after each scan.
   */
  std::set<std::string> discovered_endpoint_ids_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/client_proxy.cc"

#endif  // CORE_INTERNAL_CLIENT_PROXY_H_
