#include "core/internal/endpoint_channel_manager.h"

#include "core/internal/ble_endpoint_channel.h"
#include "core/internal/bluetooth_endpoint_channel.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
EndpointChannelManager<Platform>::EndpointChannelManager(
    Ptr<MediumManager<Platform> > medium_manager)
    : lock_(Platform::createLock()),
      medium_manager_(medium_manager),
      channel_state_(new ChannelState()) {}

template <typename Platform>
EndpointChannelManager<Platform>::~EndpointChannelManager() {
  Synchronized s(lock_.get());

  // TODO(tracyzhou): logger.atDebug().log("Initiating shutdown of
  // EndpointChannelManager.")
  channel_state_.destroy();
  // TODO(tracyzhou): logger.atDebug().log("EndpointChannelManager has shut
  // down.");
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::createOutgoingBluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket) {
  return BluetoothEndpointChannel<Platform>::createOutgoing(
      medium_manager_, channel_name, bluetooth_socket);
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::createIncomingBluetoothEndpointChannel(
    const string& channel_name, Ptr<BluetoothSocket> bluetooth_socket) {
  return BluetoothEndpointChannel<Platform>::createIncoming(
      medium_manager_, channel_name, bluetooth_socket);
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::createOutgoingBLEEndpointChannel(
    const string& channel_name, Ptr<BLESocket> ble_socket) {
  return BLEEndpointChannel<Platform>::createOutgoing(medium_manager_,
                                                      channel_name, ble_socket);
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::createIncomingBLEEndpointChannel(
    const string& channel_name, Ptr<BLESocket> ble_socket) {
  return BLEEndpointChannel<Platform>::createIncoming(medium_manager_,
                                                      channel_name, ble_socket);
}

template <typename Platform>
void EndpointChannelManager<Platform>::registerChannelForEndpoint(
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
template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::replaceChannelForEndpoint(
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

template <typename Platform>
bool EndpointChannelManager<Platform>::encryptChannelForEndpoint(
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

template <typename Platform>
Ptr<EndpointChannel> EndpointChannelManager<Platform>::getChannelForEndpoint(
    const string& endpoint_id) {
  Synchronized s(lock_.get());

  return channel_state_->getChannelForEndpoint(endpoint_id);
}

template <typename Platform>
void EndpointChannelManager<Platform>::setActiveEndpointChannel(
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

template <typename Platform>
void EndpointChannelManager<Platform>::encryptChannel(
    const string& endpoint_id, Ptr<EndpointChannel> endpoint_channel,
    Ptr<securegcm::D2DConnectionContextV1> encryption_context) {
  // TODO(tracyzhou): Add logging.
  endpoint_channel->enableEncryption(encryption_context);
}

///////////////////////////////// ChannelState /////////////////////////////////

template <typename Platform>
EndpointChannelManager<Platform>::ChannelState::~ChannelState() {
  while (!endpoint_id_to_metadata_.empty()) {
    typename EndpointIdToMetadataMap::iterator it =
        endpoint_id_to_metadata_.begin();
    // TODO(tracyzhou): Add logging.
    removeEndpoint(it->first,
                   proto::connections::DisconnectionReason::SHUTDOWN);
  }
}

template <typename Platform>
bool EndpointChannelManager<Platform>::ChannelState::isEndpointEncrypted(
    const string& endpoint_id) {
  return !getEncryptionContextForEndpoint(endpoint_id).isNull();
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::ChannelState::updateChannelForEndpoint(
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

  // Upgrade endpoint_channel to be reference-counted before starting to track
  // it (and make it clear that endpoint_channel no longer owns the raw
  // pointer).
  endpoint_metadata->endpoint_channel = MakeRefCountedPtr(&(*endpoint_channel));
  endpoint_channel.clear();
  endpoint_id_to_metadata_[endpoint_id] = endpoint_metadata;

  return scoped_previous_endpoint_channel.release();
}

template <typename Platform>
Ptr<securegcm::D2DConnectionContextV1> EndpointChannelManager<Platform>::
    ChannelState::updateEncryptionContextForEndpoint(
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

template <typename Platform>
bool EndpointChannelManager<Platform>::ChannelState::removeEndpoint(
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

template <typename Platform>
Ptr<securegcm::D2DConnectionContextV1>
EndpointChannelManager<Platform>::ChannelState::getEncryptionContextForEndpoint(
    const string& endpoint_id) {
  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    return Ptr<securegcm::D2DConnectionContextV1>();
  }

  return it->second->encryption_context;
}

template <typename Platform>
Ptr<EndpointChannel>
EndpointChannelManager<Platform>::ChannelState::getChannelForEndpoint(
    const string& endpoint_id) {
  typename EndpointIdToMetadataMap::iterator it =
      endpoint_id_to_metadata_.find(endpoint_id);
  if (it == endpoint_id_to_metadata_.end()) {
    return Ptr<EndpointChannel>();
  }

  return it->second->endpoint_channel;
}

template <typename Platform>
bool EndpointChannelManager<Platform>::unregisterChannelForEndpoint(
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
