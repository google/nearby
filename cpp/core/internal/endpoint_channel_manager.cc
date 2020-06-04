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

#include "core/internal/endpoint_channel_manager.h"

#include "core/internal/ble_endpoint_channel.h"
#include "core/internal/bluetooth_endpoint_channel.h"
#include "core/internal/wifi_lan_endpoint_channel.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

EndpointChannelManager::EndpointChannelManager(
    Ptr<MediumManager<Platform> > medium_manager)
    : lock_(Platform::createLock()),
      medium_manager_(medium_manager),
      channel_state_(new ChannelState()) {}

EndpointChannelManager::~EndpointChannelManager() {
  Synchronized s(lock_.get());

  // TODO(tracyzhou): logger.atDebug().log("Initiating shutdown of
  // EndpointChannelManager.")
  channel_state_.destroy();
  // TODO(tracyzhou): logger.atDebug().log("EndpointChannelManager has shut
  // down.");
}

Ptr<EndpointChannel>
EndpointChannelManager::createOutgoingBluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket) {
  return BluetoothEndpointChannel::createOutgoing(medium_manager_, channel_name,
                                                  bluetooth_socket);
}

Ptr<EndpointChannel>
EndpointChannelManager::createIncomingBluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket) {
  return BluetoothEndpointChannel::createIncoming(medium_manager_, channel_name,
                                                  bluetooth_socket);
}

Ptr<EndpointChannel> EndpointChannelManager::createOutgoingBLEEndpointChannel(
    const string& channel_name, Ptr<BLESocket> ble_socket) {
  return BLEEndpointChannel::createOutgoing(medium_manager_, channel_name,
                                            ble_socket);
}

Ptr<EndpointChannel> EndpointChannelManager::createIncomingBLEEndpointChannel(
    const string& channel_name, Ptr<BLESocket> ble_socket) {
  return BLEEndpointChannel::createIncoming(medium_manager_, channel_name,
                                            ble_socket);
}

Ptr<EndpointChannel>
EndpointChannelManager::CreateOutgoingWifiLanEndpointChannel(
    const string& channel_name, Ptr<WifiLanSocket> wifi_lan_socket) {
  return WifiLanEndpointChannel::CreateOutgoing(
      medium_manager_, channel_name, wifi_lan_socket);
}

Ptr<EndpointChannel>
EndpointChannelManager::CreateIncomingWifiLanEndpointChannel(
    const string& channel_name, Ptr<WifiLanSocket> wifi_lan_socket) {
  return WifiLanEndpointChannel::CreateIncoming(
      medium_manager_, channel_name, wifi_lan_socket);
}

void EndpointChannelManager::registerChannelForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> endpoint_channel) {
  Synchronized s(lock_.get());

  // Just in case there was a previous channel, unregister (and, thus, close) it
  // now.
  unregisterChannelForEndpoint(endpoint_id);

  setActiveEndpointChannel(client_proxy, endpoint_id, endpoint_channel);

  // TODO(tracyzhou): Add logging.
}

#ifdef BANDWIDTH_UPGRADE_MANAGER_IMPLEMENTED
Ptr<EndpointChannel> EndpointChannelManager::replaceChannelForEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> endpoint_channel) {
  Synchronized s(lock_.get());

  ScopedPtr<Ptr<EndpointChannel> > scoped_previous_endpoint_channel(
      channel_state_->getChannelForEndpoint(endpoint_id));
  if (scoped_previous_endpoint_channel.isNull()) {
    // TODO(tracyzhou): Add logging.
    return Ptr<EndpointChannel>();
  }

  setActiveEndpointChannel(client_proxy, endpoint_id, endpoint_channel);

  // TODO(tracyzhou): Add logging.

  return scoped_previous_endpoint_channel.release();
}
#endif

bool EndpointChannelManager::encryptChannelForEndpoint(
    const string& endpoint_id,
    Ptr<securegcm::D2DConnectionContextV1> encryption_context) {
  Synchronized s(lock_.get());

  ScopedPtr<Ptr<EndpointChannel> > scoped_endpoint_channel(
      channel_state_->getChannelForEndpoint(endpoint_id));
  if (scoped_endpoint_channel.isNull()) {
    // TODO(tracyzhou): Add logging.
    return false;
  }

  // We found the requested EndpointChannel, so encrypt it.
  encryptChannel(endpoint_id, scoped_endpoint_channel.get(),
                 encryption_context);

  // Then update 'endpoint_id' to use this new 'encryption_context' here
  // onwards.
  //
  // Remember to manage the memory of the returned
  // Ptr<securegcm::D2DConnectionContextV1> responsibly, even though we don't
  // need what's returned.
  ScopedPtr<Ptr<securegcm::D2DConnectionContextV1> >(
      channel_state_->updateEncryptionContextForEndpoint(endpoint_id,
                                                         encryption_context));
  return true;
}

Ptr<EndpointChannel> EndpointChannelManager::getChannelForEndpoint(
    const string& endpoint_id) {
  Synchronized s(lock_.get());

  return channel_state_->getChannelForEndpoint(endpoint_id);
}

void EndpointChannelManager::setActiveEndpointChannel(
    Ptr<ClientProxy<Platform> > client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> endpoint_channel) {
#ifdef BANDWIDTH_UPGRADE_MANAGER_IMPLEMENTED
  // If the endpoint is currently encrypted, encrypt this new
  // 'endpoint_channel'.
  if (channel_state_->isEndpointEncrypted(endpoint_id)) {
    encryptChannel(
        endpoint_id, endpoint_channel,
        channel_state_->getEncryptionContextForEndpoint(endpoint_id));
  }
#endif

  // Then update 'endpoint_id' to use this new 'endpoint_channel' here onwards.
  //
  // Remember to manage the memory of the returned Ptr<EndpointChannel>
  // responsibly, even though we don't need what's returned.
  ScopedPtr<Ptr<EndpointChannel> >(
      channel_state_->updateChannelForEndpoint(endpoint_id, endpoint_channel));
}

void EndpointChannelManager::encryptChannel(
    const string& endpoint_id, Ptr<EndpointChannel> endpoint_channel,
    Ptr<securegcm::D2DConnectionContextV1> encryption_context) {
  // TODO(tracyzhou): Add logging.
  endpoint_channel->enableEncryption(encryption_context);
}

///////////////////////////////// ChannelState /////////////////////////////////

EndpointChannelManager::ChannelState::~ChannelState() {
  while (!endpoint_id_to_metadata_.empty()) {
    typename EndpointIdToMetadataMap::iterator it =
        endpoint_id_to_metadata_.begin();
    // TODO(tracyzhou): Add logging.
    removeEndpoint(it->first,
                   proto::connections::DisconnectionReason::SHUTDOWN);
  }
}

bool EndpointChannelManager::ChannelState::isEndpointEncrypted(
    const string& endpoint_id) {
  return !getEncryptionContextForEndpoint(endpoint_id).isNull();
}

Ptr<EndpointChannel>
EndpointChannelManager::ChannelState::updateChannelForEndpoint(
    const string& endpoint_id, Ptr<EndpointChannel> endpoint_channel) {
  Ptr<EndpointChannel> previous_endpoint_channel;
  Ptr<EndpointMetaData> endpoint_metadata;

  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    endpoint_metadata = MakePtr(new EndpointMetaData());
  } else {
    endpoint_metadata = it->second;
    previous_endpoint_channel = endpoint_metadata->endpoint_channel;
  }
  // Avoid leaks.
  ScopedPtr<Ptr<EndpointChannel> > scoped_previous_endpoint_channel(
      previous_endpoint_channel);

  endpoint_metadata->endpoint_channel = endpoint_channel;
  endpoint_channel.clear();
  endpoint_id_to_metadata_[endpoint_id] = endpoint_metadata;

  return scoped_previous_endpoint_channel.release();
}

Ptr<securegcm::D2DConnectionContextV1>
EndpointChannelManager::ChannelState::updateEncryptionContextForEndpoint(
    const string& endpoint_id,
    Ptr<securegcm::D2DConnectionContextV1> encryption_context) {
  Ptr<securegcm::D2DConnectionContextV1> previous_encryption_context;
  Ptr<EndpointMetaData> endpoint_metadata;

  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    endpoint_metadata = MakePtr(new EndpointMetaData());
  } else {
    endpoint_metadata = it->second;
    previous_encryption_context = endpoint_metadata->encryption_context;
  }
  // Avoid leaks.
  ScopedPtr<Ptr<securegcm::D2DConnectionContextV1> >
      scoped_previous_encryption_context(previous_encryption_context);

  endpoint_metadata->encryption_context = encryption_context;
  endpoint_id_to_metadata_[endpoint_id] = endpoint_metadata;

  return scoped_previous_encryption_context.release();
}

bool EndpointChannelManager::ChannelState::removeEndpoint(
    const string& endpoint_id, proto::connections::DisconnectionReason reason) {
  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    return false;
  }

  it->second->endpoint_channel->close(reason);
  it->second.destroy();
  endpoint_id_to_metadata_.erase(it);
  return true;
}

Ptr<securegcm::D2DConnectionContextV1>
EndpointChannelManager::ChannelState::getEncryptionContextForEndpoint(
    const string& endpoint_id) {
  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    return Ptr<securegcm::D2DConnectionContextV1>();
  }

  return it->second->encryption_context;
}

Ptr<EndpointChannel>
EndpointChannelManager::ChannelState::getChannelForEndpoint(
    const string& endpoint_id) {
  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    return Ptr<EndpointChannel>();
  }

  return it->second->endpoint_channel;
}

bool EndpointChannelManager::unregisterChannelForEndpoint(
    const string& endpoint_id) {
  Synchronized s(lock_.get());

  if (!channel_state_->removeEndpoint(
          endpoint_id,
          proto::connections::DisconnectionReason::LOCAL_DISCONNECTION)) {
    return false;
  }

  // TODO(tracyzhou): Add logging.

  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
