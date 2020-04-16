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

#include "core/internal/client_proxy.h"

#include <cstdlib>
#include <limits>
#include <sstream>
#include <utility>

#include "platform/api/hash_utils.h"
#include "platform/base64_utils.h"
#include "platform/prng.h"
#include "platform/synchronized.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

namespace client_proxy {

template <typename K, typename V>
void eraseOwnedPtrFromMap(std::map<K, Ptr<V>>& m, const K& k) {
  typename std::map<K, Ptr<V>>::iterator it = m.find(k);
  if (it != m.end()) {
    it->second.destroy();
    m.erase(it);
  }
}

}  // namespace client_proxy

template <typename Platform>
const std::int32_t ClientProxy<Platform>::kEndpointIdLength = 4;

template <typename Platform>
ClientProxy<Platform>::ClientProxy()
    : lock_(Platform::createLock()), client_id_(Prng().nextInt64()) {}

template <typename Platform>
ClientProxy<Platform>::~ClientProxy() {
  reset();
}

template <typename Platform>
std::int64_t ClientProxy<Platform>::getClientId() const {
  return client_id_;
}

template <typename Platform>
std::string ClientProxy<Platform>::generateLocalEndpointId() {
  // 1) Concatenate the DeviceID with this ClientID.
  // 2) Compute a hash of that concatenation.
  // 3) Base64-encode that hash, to make it human-readable.
  // 4) Use only the first 4 bytes of that Base64 encoding.

  std::ostringstream client_id_str;
  client_id_str << getClientId();

  ScopedPtr<Ptr<HashUtils>> hash_utils(Platform::createHashUtils());
  ScopedPtr<ConstPtr<ByteArray>> id_hash(
      hash_utils->sha256(Platform::getDeviceId() + client_id_str.str()));

  return Base64Utils::encode(id_hash.get()).substr(0, kEndpointIdLength);
}

template <typename Platform>
void ClientProxy<Platform>::reset() {
  Synchronized s(lock_.get());

  stoppedAdvertising();
  stoppedDiscovery();
  removeAllEndpoints();
}

template <typename Platform>
void ClientProxy<Platform>::startedAdvertising(
    const std::string& service_id, const Strategy& strategy,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
    const std::vector<proto::connections::Medium>& mediums) {
  Synchronized s(lock_.get());

  advertising_info_.destroy();
  advertising_info_ =
      MakePtr(new AdvertisingInfo(service_id, connection_lifecycle_listener));
}

template <typename Platform>
void ClientProxy<Platform>::stoppedAdvertising() {
  Synchronized s(lock_.get());

  if (isAdvertising()) {
    advertising_info_.destroy();
  }
}

template <typename Platform>
bool ClientProxy<Platform>::isAdvertising() {
  Synchronized s(lock_.get());

  return !advertising_info_.isNull();
}

template <typename Platform>
std::string ClientProxy<Platform>::getAdvertisingServiceId() {
  Synchronized s(lock_.get());

  if (!isAdvertising()) {
    return "";
  }

  return advertising_info_->service_id;
}

template <typename Platform>
void ClientProxy<Platform>::startedDiscovery(
    const std::string& service_id, const Strategy& strategy,
    Ptr<DiscoveryListener> discovery_listener,
    const std::vector<proto::connections::Medium>& mediums) {
  Synchronized s(lock_.get());

  discovery_info_.destroy();
  discovery_info_ = MakePtr(new DiscoveryInfo(service_id, discovery_listener));
}

template <typename Platform>
void ClientProxy<Platform>::stoppedDiscovery() {
  Synchronized s(lock_.get());

  if (isDiscovering()) {
    discovered_endpoint_ids_.clear();
    discovery_info_.destroy();
  }
}

template <typename Platform>
bool ClientProxy<Platform>::isDiscoveringServiceId(
    const std::string& service_id) {
  Synchronized s(lock_.get());

  return isDiscovering() && service_id == discovery_info_->service_id;
}

template <typename Platform>
bool ClientProxy<Platform>::isDiscovering() {
  Synchronized s(lock_.get());

  return !discovery_info_.isNull();
}

template <typename Platform>
std::string ClientProxy<Platform>::getDiscoveryServiceId() {
  Synchronized s(lock_.get());

  if (!isDiscovering()) {
    return "";
  }

  return discovery_info_->service_id;
}

template <typename Platform>
void ClientProxy<Platform>::onEndpointFound(const std::string& endpoint_id,
                                            const std::string& service_id,
                                            const std::string& endpoint_name,
                                            proto::connections::Medium medium) {
  Synchronized s(lock_.get());

  if (isDiscoveringServiceId(service_id)) {
    if (discovered_endpoint_ids_.find(endpoint_id) !=
        discovered_endpoint_ids_.end()) {
      // TODO(tracyzhou): Add logging.
      return;
    }
    discovered_endpoint_ids_.insert(endpoint_id);
    discovery_info_->discovery_listener->onEndpointFound(MakeConstPtr(
        new OnEndpointFoundParams(endpoint_id, service_id, endpoint_name)));
  }
}

template <typename Platform>
void ClientProxy<Platform>::onEndpointLost(const std::string& service_id,
                                           const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  if (isDiscoveringServiceId(service_id)) {
    std::set<std::string>::const_iterator it =
        discovered_endpoint_ids_.find(endpoint_id);
    if (it == discovered_endpoint_ids_.end()) {
      return;
    }
    discovered_endpoint_ids_.erase(it);
    discovery_info_->discovery_listener->onEndpointLost(
        MakeConstPtr(new OnEndpointLostParams(endpoint_id)));
  }
}

template <typename Platform>
void ClientProxy<Platform>::onConnectionInitiated(
    const std::string& endpoint_id, const std::string& endpoint_name,
    const std::string& authentication_token,
    ConstPtr<ByteArray> raw_authentication_token, bool is_incoming_connection,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  Synchronized s(lock_.get());

  ScopedPtr<ConstPtr<ByteArray>> scoped_raw_authentication_token(
      raw_authentication_token);

  // Whether this is incoming or outgoing, the local and remote endpoints both
  // still need to accept this connection, so set its establishment status to
  // PENDING.
  connection_establishment_statuses_.insert(
      std::make_pair(endpoint_id, ConnectionMetadata(is_incoming_connection)));

  // Remember the ConnectionLifecycleListener for this endpoint.
  connection_lifecycle_listeners_.insert(
      std::make_pair(endpoint_id, connection_lifecycle_listener));

  // Notify the client.
  //
  // Note: we allow devices to connect to an advertiser even after it stops
  // advertising, so no need to check isAdvertising() here.
  connection_lifecycle_listeners_.find(endpoint_id)
      ->second->onConnectionInitiated(
          MakeConstPtr(new OnConnectionInitiatedParams(
              endpoint_id, endpoint_name, authentication_token,
              scoped_raw_authentication_token.release(),
              is_incoming_connection)));
}

template <typename Platform>
void ClientProxy<Platform>::onConnectionResult(const std::string& endpoint_id,
                                               Status::Value status) {
  Synchronized s(lock_.get());

  if (!hasPendingConnectionToEndpoint(endpoint_id)) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  // Notify the client.
  connection_lifecycle_listeners_.find(endpoint_id)
      ->second->onConnectionResult(
          MakeConstPtr(new OnConnectionResultParams(endpoint_id, status)));
  if (Status::SUCCESS == status) {
    // Mark ourselves as connected. Payloads should now be allowed.
    typename ConnectionEstablishmentStatusesMap::iterator it =
        connection_establishment_statuses_.find(endpoint_id);
    if (it != connection_establishment_statuses_.end()) {
      it->second.status = ConnectionEstablishmentStatus::CONNECTED;
    }
  } else {
    // Otherwise, clean up.
    onDisconnected(endpoint_id, false /* notify */);
  }
}

template <typename Platform>
void ClientProxy<Platform>::onBandwidthChanged(const std::string& endpoint_id,
                                               std::int32_t quality) {
  Synchronized s(lock_.get());

  ConnectionLifecycleListenersMap::iterator it =
      connection_lifecycle_listeners_.find(endpoint_id);
  if (it != connection_lifecycle_listeners_.end()) {
    it->second->onBandwidthChanged(
        MakeConstPtr(new OnBandwidthChangedParams(endpoint_id, quality)));
  }
}

template <typename Platform>
void ClientProxy<Platform>::onDisconnected(const std::string& endpoint_id,
                                           bool notify) {
  Synchronized s(lock_.get());

  connection_establishment_statuses_.erase(endpoint_id);

  client_proxy::eraseOwnedPtrFromMap(payload_listeners_, endpoint_id);

  ConnectionLifecycleListenersMap::iterator it =
      connection_lifecycle_listeners_.find(endpoint_id);
  if (it != connection_lifecycle_listeners_.end()) {
    if (notify) {
      it->second->onDisconnected(
          MakeConstPtr(new OnDisconnectedParams(endpoint_id)));
    }
    it->second.destroy();
    connection_lifecycle_listeners_.erase(it);
  }
}

template <typename Platform>
bool ClientProxy<Platform>::isConnectedToEndpoint(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  typename ConnectionEstablishmentStatusesMap::iterator it =
      connection_establishment_statuses_.find(endpoint_id);
  if (it == connection_establishment_statuses_.end()) {
    return false;
  }
  const ConnectionMetadata& metadata = it->second;
  return metadata.status == ConnectionEstablishmentStatus::CONNECTED;
}

template <typename Platform>
std::vector<std::string> ClientProxy<Platform>::getConnectedEndpoints() {
  Synchronized s(lock_.get());

  std::vector<std::string> connected_endpoints;

  for (typename ConnectionEstablishmentStatusesMap::iterator it =
           connection_establishment_statuses_.begin();
       it != connection_establishment_statuses_.end(); it++) {
    const std::string& endpoint_id = it->first;
    const ConnectionMetadata& metadata = it->second;
    if (ConnectionEstablishmentStatus::CONNECTED == metadata.status) {
      connected_endpoints.push_back(endpoint_id);
    }
  }
  return connected_endpoints;
}

template <typename Platform>
std::vector<std::string> ClientProxy<Platform>::getPendingConnectedEndpoints() {
  Synchronized s(lock_.get());

  std::vector<std::string> pending_connected_endpoints;

  for (typename ConnectionEstablishmentStatusesMap::iterator it =
           connection_establishment_statuses_.begin();
       it != connection_establishment_statuses_.end(); it++) {
    const std::string& endpoint_id = it->first;
    const ConnectionMetadata& metadata = it->second;
    if (ConnectionEstablishmentStatus::CONNECTED != metadata.status) {
      pending_connected_endpoints.push_back(endpoint_id);
    }
  }
  return pending_connected_endpoints;
}

template <typename Platform>
std::int32_t ClientProxy<Platform>::getNumOutgoingConnections() {
  Synchronized s(lock_.get());

  std::int32_t num_outgoing_connections = 0;

  for (typename ConnectionEstablishmentStatusesMap::iterator it =
           connection_establishment_statuses_.begin();
       it != connection_establishment_statuses_.end(); it++) {
    const ConnectionMetadata& metadata = it->second;
    if (ConnectionEstablishmentStatus::CONNECTED == metadata.status &&
        !metadata.is_incoming) {
      num_outgoing_connections++;
    }
  }
  return num_outgoing_connections;
}

template <typename Platform>
std::int32_t ClientProxy<Platform>::getNumIncomingConnections() {
  Synchronized s(lock_.get());

  std::int32_t num_incoming_connections = 0;

  for (typename ConnectionEstablishmentStatusesMap::iterator it =
           connection_establishment_statuses_.begin();
       it != connection_establishment_statuses_.end(); it++) {
    const ConnectionMetadata& metadata = it->second;
    if (ConnectionEstablishmentStatus::CONNECTED == metadata.status &&
        metadata.is_incoming) {
      num_incoming_connections++;
    }
  }
  return num_incoming_connections;
}

template <typename Platform>
bool ClientProxy<Platform>::hasPendingConnectionToEndpoint(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  typename ConnectionEstablishmentStatusesMap::iterator it =
      connection_establishment_statuses_.find(endpoint_id);
  if (it == connection_establishment_statuses_.end()) {
    return false;
  }
  const ConnectionMetadata& metadata = it->second;
  return metadata.status != ConnectionEstablishmentStatus::CONNECTED;
}

template <typename Platform>
bool ClientProxy<Platform>::hasLocalEndpointResponded(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  return connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::LOCAL_ENDPOINT_ACCEPTED) ||
         connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::LOCAL_ENDPOINT_REJECTED);
}

template <typename Platform>
bool ClientProxy<Platform>::hasRemoteEndpointResponded(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  return connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::REMOTE_ENDPOINT_ACCEPTED) ||
         connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::REMOTE_ENDPOINT_REJECTED);
}

template <typename Platform>
void ClientProxy<Platform>::localEndpointAcceptedConnection(
    const std::string& endpoint_id, Ptr<PayloadListener> payload_listener) {
  Synchronized s(lock_.get());

  if (hasLocalEndpointResponded(endpoint_id)) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  appendConnectionEstablishmentStatus(
      endpoint_id, ConnectionEstablishmentStatus::LOCAL_ENDPOINT_ACCEPTED);
  payload_listeners_.insert(std::make_pair(endpoint_id, payload_listener));
}

template <typename Platform>
void ClientProxy<Platform>::localEndpointRejectedConnection(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  if (hasLocalEndpointResponded(endpoint_id)) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  appendConnectionEstablishmentStatus(
      endpoint_id, ConnectionEstablishmentStatus::LOCAL_ENDPOINT_REJECTED);
}

template <typename Platform>
void ClientProxy<Platform>::remoteEndpointAcceptedConnection(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  if (hasRemoteEndpointResponded(endpoint_id)) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  appendConnectionEstablishmentStatus(
      endpoint_id, ConnectionEstablishmentStatus::REMOTE_ENDPOINT_ACCEPTED);
}

template <typename Platform>
void ClientProxy<Platform>::remoteEndpointRejectedConnection(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  if (hasRemoteEndpointResponded(endpoint_id)) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  appendConnectionEstablishmentStatus(
      endpoint_id, ConnectionEstablishmentStatus::REMOTE_ENDPOINT_REJECTED);
}

template <typename Platform>
bool ClientProxy<Platform>::isConnectionAccepted(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  return connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::LOCAL_ENDPOINT_ACCEPTED) &&
         connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::REMOTE_ENDPOINT_ACCEPTED);
}

template <typename Platform>
bool ClientProxy<Platform>::isConnectionRejected(
    const std::string& endpoint_id) {
  Synchronized s(lock_.get());

  return connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::LOCAL_ENDPOINT_REJECTED) ||
         connectionEstablishmentStatusesContains(
             endpoint_id,
             ConnectionEstablishmentStatus::REMOTE_ENDPOINT_REJECTED);
}

template <typename Platform>
void ClientProxy<Platform>::onPayloadReceived(const std::string& endpoint_id,
                                              ConstPtr<Payload> payload) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<ConstPtr<Payload>> scoped_payload(payload);

  if (isConnectedToEndpoint(endpoint_id)) {
    payload_listeners_.find(endpoint_id)
        ->second->onPayloadReceived(MakeConstPtr(new OnPayloadReceivedParams(
            endpoint_id, scoped_payload.release())));
  }
}

template <typename Platform>
void ClientProxy<Platform>::onPayloadTransferUpdate(
    const std::string& endpoint_id,
    const PayloadTransferUpdate& payload_transfer_update) {
  Synchronized s(lock_.get());

  if (isConnectedToEndpoint(endpoint_id)) {
    payload_listeners_.find(endpoint_id)
        ->second->onPayloadTransferUpdate(
            MakeConstPtr(new OnPayloadTransferUpdateParams(
                endpoint_id, payload_transfer_update)));
  }
}

template <typename Platform>
bool ClientProxy<Platform>::operator==(const ClientProxy<Platform>& rhs) {
  return this->getClientId() == rhs.getClientId();
}

template <typename Platform>
bool ClientProxy<Platform>::operator<(const ClientProxy<Platform>& rhs) {
  return this->getClientId() < rhs.getClientId();
}

template <typename Platform>
void ClientProxy<Platform>::removeAllEndpoints() {
  Synchronized s(lock_.get());

  // Note: we may want to notify the client of onDisconnected() for each
  // endpoint, in the case when this is called from stopAllEndpoints(). For now,
  // just remove without notifying.
  for (ConnectionLifecycleListenersMap::iterator it =
           connection_lifecycle_listeners_.begin();
       it != connection_lifecycle_listeners_.end(); it++) {
    it->second.destroy();
  }
  connection_lifecycle_listeners_.clear();

  for (PayloadListenersMap::iterator it = payload_listeners_.begin();
       it != payload_listeners_.end(); it++) {
    it->second.destroy();
  }
  payload_listeners_.clear();

  connection_establishment_statuses_.clear();
}

template <typename Platform>
bool ClientProxy<Platform>::connectionEstablishmentStatusesContains(
    const std::string& endpoint_id,
    typename ConnectionEstablishmentStatus::Value status_to_match) {
  typename ConnectionEstablishmentStatusesMap::iterator it =
      connection_establishment_statuses_.find(endpoint_id);
  if (it == connection_establishment_statuses_.end()) {
    return false;
  }
  const ConnectionMetadata& metadata = it->second;
  return (metadata.status & status_to_match) != 0;
}

template <typename Platform>
void ClientProxy<Platform>::appendConnectionEstablishmentStatus(
    const std::string& endpoint_id,
    typename ConnectionEstablishmentStatus::Value status_to_append) {
  typename ConnectionEstablishmentStatusesMap::iterator it =
      connection_establishment_statuses_.find(endpoint_id);
  if (it == connection_establishment_statuses_.end()) {
    return;
  }
  ConnectionMetadata& metadata = it->second;
  metadata.status = static_cast<typename ConnectionEstablishmentStatus::Value>(
      metadata.status | status_to_append);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
