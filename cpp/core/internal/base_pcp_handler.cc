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

#include "core/internal/base_pcp_handler.h"

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <limits>

namespace location {
namespace nearby {
namespace connections {

namespace base_pcp_handler {

// TODO(reznor): Implement this method in-terms-of removeOwnedPtrFromMap()
// below.
template <typename K, typename V>
void eraseOwnedPtrFromMap(std::map<K, Ptr<V>>& m, const K& k) {
  typename std::map<K, Ptr<V>>::iterator it = m.find(k);
  if (it != m.end()) {
    it->second.destroy();
    m.erase(it);
  }
}

template <typename K, typename V>
Ptr<V> removeOwnedPtrFromMap(std::map<K, Ptr<V>>& m, const K& k) {
  Ptr<V> removed_ptr;

  typename std::map<K, Ptr<V>>::iterator it = m.find(k);
  if (it != m.end()) {
    removed_ptr = it->second;
    m.erase(it);
  }

  return removed_ptr;
}

template <typename Platform>
class StartAdvertisingCallable : public Callable<Status::Value> {
 public:
  StartAdvertisingCallable(
      Ptr<BasePCPHandler<Platform>> base_pcp_handler,
      Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
      const string& local_endpoint_name, const AdvertisingOptions& options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        service_id_(service_id),
        local_endpoint_name_(local_endpoint_name),
        options_(options),
        connection_lifecycle_listener_(connection_lifecycle_listener) {}

  ExceptionOr<Status::Value> call() override {
    // Ask the implementation to attempt to start advertising.
    ScopedPtr<Ptr<typename BasePCPHandler<Platform>::StartOperationResult>>
        result(base_pcp_handler_->startAdvertisingImpl(
            client_proxy_, service_id_,
            client_proxy_->generateLocalEndpointId(), local_endpoint_name_,
            options_));
    if (Status::SUCCESS != result->status_) {
      return ExceptionOr<Status::Value>(result->status_);
    }

    // Now that we've succeeded, mark the client as advertising.
    // Previous advertising_options_ and
    // advertising_connection_lifecycle_listener_ is not destroyed here because
    // stopAdvertising() is expected to be called before startAdvertising().
    base_pcp_handler_->advertising_options_ =
        MakePtr(new AdvertisingOptions(options_));
    base_pcp_handler_->advertising_connection_lifecycle_listener_ =
        connection_lifecycle_listener_;
    client_proxy_->startedAdvertising(
        service_id_, base_pcp_handler_->getStrategy(),
        connection_lifecycle_listener_, result->mediums_);
    return ExceptionOr<Status::Value>(Status::SUCCESS);
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string service_id_;
  const string local_endpoint_name_;
  const AdvertisingOptions options_;
  Ptr<ConnectionLifecycleListener> connection_lifecycle_listener_;
};

template <typename Platform>
class StopAdvertisingRunnable : public Runnable {
 public:
  StopAdvertisingRunnable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                          Ptr<ClientProxy<Platform>> client_proxy,
                          Ptr<CountDownLatch> latch)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        latch_(latch) {}

  void run() override {
    base_pcp_handler_->stopAdvertisingImpl(client_proxy_);
    client_proxy_->stoppedAdvertising();
    // base_pcp_handler_->advertising_options_ is purposefully not destroyed
    // here.
    base_pcp_handler_->advertising_connection_lifecycle_listener_.destroy();
    latch_->countDown();
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  Ptr<CountDownLatch> latch_;
};

template <typename Platform>
class StartDiscoveryCallable : public Callable<Status::Value> {
 public:
  StartDiscoveryCallable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                         Ptr<ClientProxy<Platform>> client_proxy,
                         const string& service_id,
                         const DiscoveryOptions& options,
                         Ptr<DiscoveryListener> discovery_listener)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        service_id_(service_id),
        options_(options),
        discovery_listener_(discovery_listener) {}

  ExceptionOr<Status::Value> call() override {
    // Ask the implementation to attempt to start discovery.
    ScopedPtr<Ptr<typename BasePCPHandler<Platform>::StartOperationResult>>
        result(base_pcp_handler_->startDiscoveryImpl(client_proxy_, service_id_,
                                                     options_));
    if (Status::SUCCESS != result->status_) {
      return ExceptionOr<Status::Value>(result->status_);
    }

    // Now that we've succeeded, mark the client as discovering and clear out
    // any old endpoints we had discovered.
    // Previous discovery_options_ is not destroyed here because stopDiscovery()
    // is expected to be called before startDiscovery().
    base_pcp_handler_->discovery_options_ =
        MakePtr(new DiscoveryOptions(options_));
    for (typename BasePCPHandler<Platform>::DiscoveredEndpointsMap::iterator
             it = base_pcp_handler_->discovered_endpoints_.begin();
         it != base_pcp_handler_->discovered_endpoints_.end(); it++) {
      it->second.destroy();
    }
    base_pcp_handler_->discovered_endpoints_.clear();
    client_proxy_->startedDiscovery(
        service_id_, base_pcp_handler_->getStrategy(),
        discovery_listener_.release(), result->mediums_);
    return ExceptionOr<Status::Value>(Status::SUCCESS);
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string service_id_;
  const DiscoveryOptions options_;
  ScopedPtr<Ptr<DiscoveryListener>> discovery_listener_;
};

template <typename Platform>
class StopDiscoveryRunnable : public Runnable {
 public:
  StopDiscoveryRunnable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                        Ptr<ClientProxy<Platform>> client_proxy,
                        Ptr<CountDownLatch> latch)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        latch_(latch) {}

  void run() override {
    base_pcp_handler_->stopDiscoveryImpl(client_proxy_);
    client_proxy_->stoppedDiscovery();
    // base_pcp_handler_->discovery_options_ is purposefully not destroyed here.
    latch_->countDown();
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  Ptr<CountDownLatch> latch_;
};

template <typename Platform>
class RequestConnectionRunnable : public Runnable {
 public:
  RequestConnectionRunnable(
      Ptr<BasePCPHandler<Platform>> base_pcp_handler,
      Ptr<ClientProxy<Platform>> client_proxy,
      const string& local_endpoint_name, const string& endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
      Ptr<SettableFuture<Status::Value>> result)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        local_endpoint_name_(local_endpoint_name),
        endpoint_id_(endpoint_id),
        connection_lifecycle_listener_(connection_lifecycle_listener),
        result_(result) {}

  void run() override {
    std::int64_t start_time_millis =
        base_pcp_handler_->system_clock_->elapsedRealtime();

    // If we already have a pending connection, then we shouldn't allow any more
    // outgoing connections to this endpoint.
    typename BasePCPHandler<Platform>::PendingConnectionsMap::iterator it =
        base_pcp_handler_->pending_connections_.find(endpoint_id_);
    if (it != base_pcp_handler_->pending_connections_.end()) {
      // TODO(tracyzhou): Add logging.
      result_->set(Status::ALREADY_CONNECTED_TO_ENDPOINT);
      return;
    }

    // If our child class says we can't send any more outgoing connections,
    // listen to them.
    if (base_pcp_handler_->shouldEnforceTopologyConstraints() &&
        !base_pcp_handler_->canSendOutgoingConnection(client_proxy_)) {
      // TODO(tracyzhou): Add logging.
      result_->set(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> endpoint =
        base_pcp_handler_->getDiscoveredEndpoint(endpoint_id_);
    if (endpoint.isNull()) {
      // TODO(tracyzhou): Add logging.
      result_->set(Status::ENDPOINT_UNKNOWN);
      return;
    }

    typename BasePCPHandler<Platform>::ConnectImplResult connect_impl_result =
        base_pcp_handler_->connectImpl(client_proxy_, endpoint);

    if (connect_impl_result.endpoint_channel.isNull()) {
      // TODO(tracyzhou): Add logging
      base_pcp_handler_->processPreConnectionInitiationFailure(
          client_proxy_, connect_impl_result.medium, endpoint_id_,
          connect_impl_result.endpoint_channel, false /* is_incoming */,
          start_time_millis, connect_impl_result.status, result_);
      return;
    }

    ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(
        connect_impl_result.endpoint_channel);

    // TODO(tracyzhou): Add logging.
    // Generate the nonce to use for this connection.
    std::int32_t nonce = base_pcp_handler_->prng_.nextInt32();

    // The first message we have to send, after connecting, is to tell the
    // endpoint about ourselves.
    Exception::Value write_exception =
        base_pcp_handler_->writeConnectionRequestFrame(
            scoped_endpoint_channel.get(),
            client_proxy_->generateLocalEndpointId(), local_endpoint_name_,
            nonce, base_pcp_handler_->getConnectionMediumsByPriority());
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        base_pcp_handler_->processPreConnectionInitiationFailure(
            client_proxy_, scoped_endpoint_channel->getMedium(), endpoint_id_,
            scoped_endpoint_channel.get(), false /* is_incoming */,
            start_time_millis, Status::ENDPOINT_IO_ERROR, result_);
        return;
      }
    }

    // TODO(tracyzhou): Add logging.

    // We've successfully connected to the device, and are now about to jump on
    // to the EncryptionRunner thread to start running our encryption protocol.
    // We'll mark ourselves as pending in case we get another call to
    // requestConnection or onIncomingConnection, so that we can cancel the
    // connection if needed.
    Ptr<EndpointChannel> endpoint_channel =
        base_pcp_handler_->pending_connections_
            .insert(std::make_pair(
                endpoint_id_,
                BasePCPHandler<Platform>::PendingConnectionInfo::
                    newOutgoingPendingConnectionInfo(
                        client_proxy_, endpoint->getEndpointName(),
                        scoped_endpoint_channel.release(), nonce,
                        start_time_millis,
                        connection_lifecycle_listener_.release(), result_)))
            .first->second->endpoint_channel_.get();

    // Next, we'll set up encryption. When it's done, our future will return and
    // requestConnection() will finish.
    base_pcp_handler_->encryption_runner_->startClient(
        client_proxy_, endpoint_id_, endpoint_channel,
        MakePtr(new typename BasePCPHandler<Platform>::ResultListenerFacade(
            base_pcp_handler_)));
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string local_endpoint_name_;
  const string endpoint_id_;
  ScopedPtr<Ptr<ConnectionLifecycleListener>> connection_lifecycle_listener_;
  Ptr<SettableFuture<Status::Value>> result_;
};

template <typename Platform>
class AcceptConnectionCallable : public Callable<Status::Value> {
 public:
  AcceptConnectionCallable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                           Ptr<ClientProxy<Platform>> client_proxy,
                           const string& endpoint_id,
                           Ptr<PayloadListener> payload_listener)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        payload_listener_(payload_listener) {}

  ExceptionOr<Status::Value> call() override {
    // TODO(tracyzhou): Add logging.
    typename BasePCPHandler<Platform>::PendingConnectionsMap::iterator it =
        base_pcp_handler_->pending_connections_.find(endpoint_id_);
    if (it == base_pcp_handler_->pending_connections_.end()) {
      // TODO(tracyzhou): Add logging.
      return ExceptionOr<Status::Value>(Status::ENDPOINT_UNKNOWN);
    }
    Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
        connection_info = it->second;

    // By this point in the flow, connection_info->endpoint_channel_ has been
    // nulled out because ownership of that EndpointChannel was passed on to
    // EndpointChannelManager via a call to
    // EndpointManager::registerEndpoint(), so we now need to get access to the
    // EndpointChannel from the authoritative owner.
    ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(
        base_pcp_handler_->endpoint_channel_manager_->getChannelForEndpoint(
            endpoint_id_));
    if (scoped_endpoint_channel.isNull()) {
      // TODO(reznor): Add logging.
      base_pcp_handler_->processPreConnectionResultFailure(client_proxy_,
                                                           endpoint_id_);
      return ExceptionOr<Status::Value>(Status::ENDPOINT_UNKNOWN);
    }

    Exception::Value write_exception = scoped_endpoint_channel->write(
        OfflineFrames::forConnectionResponse(Status::SUCCESS));
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        // TODO(tracyzhou): Add logging.
        base_pcp_handler_->processPreConnectionResultFailure(client_proxy_,
                                                             endpoint_id_);
        return ExceptionOr<Status::Value>(Status::ENDPOINT_IO_ERROR);
      }
    }

    // TODO(tracyzhou): Add logging.
    connection_info->localEndpointAcceptedConnection(
        endpoint_id_, payload_listener_.release());
    base_pcp_handler_->evaluateConnectionResult(
        client_proxy_, endpoint_id_, false /* can_close_immediately */);
    return ExceptionOr<Status::Value>(Status::SUCCESS);
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  ScopedPtr<Ptr<PayloadListener>> payload_listener_;
};

template <typename Platform>
class RejectConnectionCallable : public Callable<Status::Value> {
 public:
  RejectConnectionCallable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                           Ptr<ClientProxy<Platform>> client_proxy,
                           const string& endpoint_id)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id) {}

  ExceptionOr<Status::Value> call() override {
    // TODO(tracyzhou): Add logging.
    typename BasePCPHandler<Platform>::PendingConnectionsMap::iterator it =
        base_pcp_handler_->pending_connections_.find(endpoint_id_);
    if (it == base_pcp_handler_->pending_connections_.end()) {
      // TODO(tracyzhou): Add logging.
      return ExceptionOr<Status::Value>(Status::ENDPOINT_UNKNOWN);
    }
    Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
        connection_info = it->second;

    // By this point in the flow, connection_info->endpoint_channel_ has been
    // nulled out because ownership of that EndpointChannel was passed on to
    // EndpointChannelManager via a call to
    // EndpointManager::registerEndpoint(), so we now need to get access to the
    // EndpointChannel from the authoritative owner.
    ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(
        base_pcp_handler_->endpoint_channel_manager_->getChannelForEndpoint(
            endpoint_id_));
    if (scoped_endpoint_channel.isNull()) {
      // TODO(reznor): Add logging.
      base_pcp_handler_->processPreConnectionResultFailure(client_proxy_,
                                                           endpoint_id_);
      return ExceptionOr<Status::Value>(Status::ENDPOINT_UNKNOWN);
    }

    Exception::Value write_exception = scoped_endpoint_channel->write(
        OfflineFrames::forConnectionResponse(Status::CONNECTION_REJECTED));
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        // TODO(tracyzhou): Add logging.
        base_pcp_handler_->processPreConnectionResultFailure(client_proxy_,
                                                             endpoint_id_);
        return ExceptionOr<Status::Value>(Status::ENDPOINT_IO_ERROR);
      }
    }

    // TODO(tracyzhou): Add logging.
    connection_info->localEndpointRejectedConnection(endpoint_id_);
    base_pcp_handler_->evaluateConnectionResult(
        client_proxy_, endpoint_id_, false /* can_close_immediately */);
    return ExceptionOr<Status::Value>(Status::SUCCESS);
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
};

class ReadConnectionRequestCancelableAlarmRunnable : public Runnable {
 public:
  explicit ReadConnectionRequestCancelableAlarmRunnable(
      Ptr<EndpointChannel> endpoint_channel)
      : endpoint_channel_(endpoint_channel) {}

  void run() override {
    // TODO(tracyzhou): Add logging.
    endpoint_channel_->close();
  }

 private:
  Ptr<EndpointChannel> endpoint_channel_;
};

template <typename Platform>
class EvaluateConnectionResultCancelableAlarmRunnable : public Runnable {
 public:
  EvaluateConnectionResultCancelableAlarmRunnable(
      Ptr<EndpointManager<Platform>> endpoint_manager,
      Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id)
      : endpoint_manager_(endpoint_manager),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id) {}

  void run() override {
    // TODO(tracyzhou): Add logging.
    endpoint_manager_->discardEndpoint(client_proxy_, endpoint_id_);
  }

 private:
  Ptr<EndpointManager<Platform>> endpoint_manager_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
};

template <typename Platform>
class ProcessEndpointDisconnectionRunnable : public Runnable {
 public:
  ProcessEndpointDisconnectionRunnable(
      Ptr<BasePCPHandler<Platform>> base_pcp_handler,
      Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
      Ptr<CountDownLatch> process_disconnection_barrier)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        process_disconnection_barrier_(process_disconnection_barrier) {}

  void run() override {
    typename BasePCPHandler<
        Platform>::PendingRejectedConnectionCloseAlarmsMap::iterator it =
        base_pcp_handler_->pending_rejected_connection_close_alarms_.find(
            endpoint_id_);
    if (it !=
        base_pcp_handler_->pending_rejected_connection_close_alarms_.end()) {
      it->second->cancel();
      it->second.destroy();
      base_pcp_handler_->pending_rejected_connection_close_alarms_.erase(it);
    }
    base_pcp_handler_->processPreConnectionResultFailure(client_proxy_,
                                                         endpoint_id_);

    process_disconnection_barrier_->countDown();
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  Ptr<CountDownLatch> process_disconnection_barrier_;
};

template <typename Platform>
class OnConnectionResponseRunnable : public Runnable {
 public:
  OnConnectionResponseRunnable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                               Ptr<ClientProxy<Platform>> client_proxy,
                               const string& endpoint_id,
                               ConstPtr<OfflineFrame> offline_frame,
                               Ptr<CountDownLatch> latch)
      : base_pcp_handler_(base_pcp_handler),
        client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        offline_frame_(offline_frame),
        latch_(latch) {}

  void run() override {
    // TODO(tracyzhou): Add logging.

    if (client_proxy_->hasRemoteEndpointResponded(endpoint_id_)) {
      // TODO(tracyzhou): Add logging.
      return;
    }

    const ConnectionResponseFrame& connection_response =
        offline_frame_->v1().connection_response();

    // TODO(tracyzhou): Assign int values to Status.
    if (Status::SUCCESS == connection_response.status()) {
      // TODO(tracyzhou): Add logging.
      client_proxy_->remoteEndpointAcceptedConnection(endpoint_id_);
    } else {
      // TODO(tracyzhou): Add logging.
      client_proxy_->remoteEndpointRejectedConnection(endpoint_id_);
    }

    base_pcp_handler_->evaluateConnectionResult(
        client_proxy_, endpoint_id_,
        /* can_close_immediately= */ true);

    latch_->countDown();
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  ScopedPtr<ConstPtr<OfflineFrame>> offline_frame_;
  Ptr<CountDownLatch> latch_;
};

template <typename Platform>
class OnEncryptionSuccessRunnable : public Runnable {
 public:
  OnEncryptionSuccessRunnable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                              const string& endpoint_id,
                              Ptr<securegcm::UKey2Handshake> ukey2_handshake,
                              const string& authentication_token,
                              ConstPtr<ByteArray> raw_authentication_token)
      : base_pcp_handler_(base_pcp_handler),
        endpoint_id_(endpoint_id),
        ukey2_handshake_(ukey2_handshake),
        authentication_token_(authentication_token),
        raw_authentication_token_(raw_authentication_token) {}

  void run() override {
    // Quick fail if we've been removed from pending connections while we were
    // busy running UKEY2.
    typename BasePCPHandler<Platform>::PendingConnectionsMap::iterator it =
        base_pcp_handler_->pending_connections_.find(endpoint_id_);
    if (it == base_pcp_handler_->pending_connections_.end()) {
      // TODO(tracyzhou): Add logging.
      return;
    }

    Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
        connection_info = it->second;
    connection_info->setUKey2Handshake(ukey2_handshake_.release());
    // TODO(tracyzhou): Add logging.

    // Set ourselves up so that we receive all acceptance/rejection messages
    base_pcp_handler_->endpoint_manager_->registerIncomingOfflineFrameProcessor(
        V1Frame::CONNECTION_RESPONSE, base_pcp_handler_);

    // Now we register our endpoint so that we can listen for both sides to
    // accept.
    base_pcp_handler_->endpoint_manager_->registerEndpoint(
        connection_info->client_proxy_, endpoint_id_,
        connection_info->remote_endpoint_name_, authentication_token_,
        raw_authentication_token_.release(), connection_info->is_incoming_,
        connection_info->endpoint_channel_.release(),
        connection_info->connection_lifecycle_listener_.release());

    if (!connection_info->request_connection_result_.isNull()) {
      connection_info->request_connection_result_->set(Status::SUCCESS);
      connection_info->request_connection_result_.clear();
    }
  }

 private:
  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  const string endpoint_id_;
  ScopedPtr<Ptr<securegcm::UKey2Handshake>> ukey2_handshake_;
  const string authentication_token_;
  ScopedPtr<ConstPtr<ByteArray>> raw_authentication_token_;
};

template <typename Platform>
class OnEncryptionFailureRunnable : public Runnable {
 public:
  OnEncryptionFailureRunnable(Ptr<BasePCPHandler<Platform>> base_pcp_handler,
                              const string& endpoint_id,
                              Ptr<EndpointChannel> endpoint_channel)
      : base_pcp_handler_(base_pcp_handler),
        endpoint_id_(endpoint_id),
        endpoint_channel_(endpoint_channel) {}

  void run() override {
    typename BasePCPHandler<Platform>::PendingConnectionsMap::iterator it =
        base_pcp_handler_->pending_connections_.find(endpoint_id_);
    if (it == base_pcp_handler_->pending_connections_.end()) {
      // TODO(tracyzhou): Add logging.
      return;
    }

    Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
        connection_info = it->second;
    // We had a bug here, caused by a race with EncryptionRunner. We now verify
    // the EndpointChannel to avoid it. In a simultaneous connection, we clean
    // up one of the two EndpointChannels and then update our pendingConnections
    // with the winning channel's state. Closing a channel that was in the
    // middle of EncryptionRunner would trigger onEncryptionFailed, and, since
    // the map had already updated with the winning EndpointChannel, we closed
    // it too by accident.
    if (!endpointChannelsAreEqual(endpoint_channel_,
                                  connection_info->endpoint_channel_.get())) {
      // TODO(tracyzhou): Add logging.
      return;
    }

    base_pcp_handler_->processPreConnectionInitiationFailure(
        connection_info->client_proxy_,
        connection_info->endpoint_channel_->getMedium(), endpoint_id_,
        connection_info->endpoint_channel_.get(), connection_info->is_incoming_,
        connection_info->start_time_millis_, Status::ENDPOINT_IO_ERROR,
        connection_info->request_connection_result_);
    connection_info->request_connection_result_.clear();
  }

 private:
  static bool endpointChannelsAreEqual(Ptr<EndpointChannel> lhs,
                                       Ptr<EndpointChannel> rhs) {
    return (lhs->getType() == rhs->getType()) &&
           (lhs->getName() == rhs->getName()) &&
           (lhs->getMedium() == rhs->getMedium());
  }

  Ptr<BasePCPHandler<Platform>> base_pcp_handler_;
  const string endpoint_id_;
  Ptr<EndpointChannel> endpoint_channel_;
};

}  // namespace base_pcp_handler

template <typename Platform>
const std::int64_t
    BasePCPHandler<Platform>::kConnectionRequestReadTimeoutMillis =
        2 * 1000;  // 2 seconds
template <typename Platform>
const std::int64_t
    BasePCPHandler<Platform>::kRejectedConnectionCloseDelayMillis =
        2 * 1000;  // 2 seconds

template <typename Platform>
BasePCPHandler<Platform>::BasePCPHandler(
    Ptr<EndpointManager<Platform>> endpoint_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager,
    Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager)
    : endpoint_manager_(endpoint_manager),
      endpoint_channel_manager_(endpoint_channel_manager),
      bandwidth_upgrade_manager_(bandwidth_upgrade_manager),
      bandwidth_upgrade_medium_(Platform::createAtomicReference(
          proto::connections::Medium::UNKNOWN_MEDIUM)),
      alarm_executor_(Platform::createScheduledExecutor()),
      serial_executor_(Platform::createSingleThreadExecutor()),
      system_clock_(Platform::createSystemClock()),
      prng_(),
      pending_connections_(),
      discovered_endpoints_(),
      pending_rejected_connection_close_alarms_(),
      advertising_options_(),
      discovery_options_(),
      encryption_runner_(MakePtr(new EncryptionRunner<Platform>())) {}

template <typename Platform>
BasePCPHandler<Platform>::~BasePCPHandler() {
  // TODO(reznor):
  // logger.atDebug().log("Initiating shutdown of PCPHandler(%s).",
  // getStrategy().getName());

  // Unregister ourselves from the IncomingOfflineFrameProcessors.
  endpoint_manager_->unregisterIncomingOfflineFrameProcessor(
      V1Frame::CONNECTION_RESPONSE,
      std::static_pointer_cast<
          typename EndpointManager<Platform>::IncomingOfflineFrameProcessor>(
          self_));

  encryption_runner_.destroy();

  // Stop all the ongoing Runnables (as gracefully as possible).
  serial_executor_->shutdown();
  alarm_executor_->shutdown();

  // With the alarmExecutor shut down already, we can safely clear out our
  // pending alarms.
  for (typename PendingRejectedConnectionCloseAlarmsMap::iterator it =
           pending_rejected_connection_close_alarms_.begin();
       it != pending_rejected_connection_close_alarms_.end(); it++) {
    it->second.destroy();
  }
  pending_rejected_connection_close_alarms_.clear();

  for (typename DiscoveredEndpointsMap::iterator it =
           discovered_endpoints_.begin();
       it != discovered_endpoints_.end(); it++) {
    it->second.destroy();
  }
  discovered_endpoints_.clear();

  // Unblock all Futures that were stored in our pendingConnections.
  for (typename PendingConnectionsMap::iterator it =
           pending_connections_.begin();
       it != pending_connections_.end(); it++) {
    it->second.destroy();
  }
  pending_connections_.clear();

  // TODO(reznor):
  // logger.atVerbose().log("PCPHandler(%s) has shut down.",
  // getStrategy().getName());
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::startAdvertising(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    const string& local_endpoint_name,
    const AdvertisingOptions& advertising_options,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  ScopedPtr<Ptr<Future<Status::Value>>> result(
      runOnPCPHandlerThread<Status::Value>(
          MakePtr(new base_pcp_handler::StartAdvertisingCallable<Platform>(
              self_, client_proxy, service_id, local_endpoint_name,
              advertising_options, connection_lifecycle_listener))));
  return waitForResult("startAdvertising(" + local_endpoint_name + ")",
                       client_proxy->getClientId(), result.get());
}

template <typename Platform>
void BasePCPHandler<Platform>::stopAdvertising(
    Ptr<ClientProxy<Platform>> client_proxy) {
  ScopedPtr<Ptr<CountDownLatch>> latch(Platform::createCountDownLatch(1));
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::StopAdvertisingRunnable<Platform>(
          self_, client_proxy, latch.get())));
  waitForLatch("stopAdvertising", latch.get());
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::startDiscovery(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    const DiscoveryOptions& discovery_options,
    Ptr<DiscoveryListener> discovery_listener) {
  ScopedPtr<Ptr<Future<Status::Value>>> result(
      runOnPCPHandlerThread<Status::Value>(
          MakePtr(new base_pcp_handler::StartDiscoveryCallable<Platform>(
              self_, client_proxy, service_id, discovery_options,
              discovery_listener))));
  return waitForResult("startDiscovery(" + service_id + ")",
                       client_proxy->getClientId(), result.get());
}

template <typename Platform>
void BasePCPHandler<Platform>::stopDiscovery(
    Ptr<ClientProxy<Platform>> client_proxy) {
  ScopedPtr<Ptr<CountDownLatch>> latch(Platform::createCountDownLatch(1));
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::StopDiscoveryRunnable<Platform>(
          self_, client_proxy, latch.get())));
  waitForLatch("stopDiscovery", latch.get());
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::requestConnection(
    Ptr<ClientProxy<Platform>> client_proxy, const string& local_endpoint_name,
    const string& endpoint_id,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener) {
  ScopedPtr<Ptr<SettableFuture<Status::Value>>> result(
      Platform::template createSettableFuture<Status::Value>());
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::RequestConnectionRunnable<Platform>(
          self_, client_proxy, local_endpoint_name, endpoint_id,
          connection_lifecycle_listener, result.get())));
  return waitForResult("requestConnection(" + endpoint_id + ")",
                       client_proxy->getClientId(), result.get());
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::acceptConnection(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    Ptr<PayloadListener> payload_listener) {
  ScopedPtr<Ptr<Future<Status::Value>>> result(
      runOnPCPHandlerThread<Status::Value>(
          MakePtr(new base_pcp_handler::AcceptConnectionCallable<Platform>(
              self_, client_proxy, endpoint_id, payload_listener))));
  return waitForResult("acceptConnection(" + endpoint_id + ")",
                       client_proxy->getClientId(), result.get());
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::rejectConnection(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id) {
  ScopedPtr<Ptr<Future<Status::Value>>> result(
      runOnPCPHandlerThread<Status::Value>(
          MakePtr(new base_pcp_handler::RejectConnectionCallable<Platform>(
              self_, client_proxy, endpoint_id))));
  return waitForResult("rejectConnection(" + endpoint_id + ")",
                       client_proxy->getClientId(), result.get());
}

template <typename Platform>
proto::connections::Medium
BasePCPHandler<Platform>::getBandwidthUpgradeMedium() {
  return bandwidth_upgrade_medium_->get();
}

template <typename Platform>
void BasePCPHandler<Platform>::processIncomingOfflineFrame(
    ConstPtr<OfflineFrame> offline_frame, const string& from_endpoint_id,
    Ptr<ClientProxy<Platform>> to_client_proxy,
    proto::connections::Medium current_medium) {
  onConnectionResponse(to_client_proxy, from_endpoint_id, offline_frame);
}

template <typename Platform>
void BasePCPHandler<Platform>::processEndpointDisconnection(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    Ptr<CountDownLatch> process_disconnection_barrier) {
  runOnPCPHandlerThread(MakePtr(
      new base_pcp_handler::ProcessEndpointDisconnectionRunnable<Platform>(
          self_, client_proxy, endpoint_id, process_disconnection_barrier)));
}

template <typename Platform>
void BasePCPHandler<Platform>::onEncryptionSuccessImpl(
    const string& endpoint_id, Ptr<securegcm::UKey2Handshake> ukey2_handshake,
    const string& authentication_token,
    ConstPtr<ByteArray> raw_authentication_token) {
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::OnEncryptionSuccessRunnable<Platform>(
          self_, endpoint_id, ukey2_handshake, authentication_token,
          raw_authentication_token)));
}

template <typename Platform>
void BasePCPHandler<Platform>::onEncryptionFailureImpl(
    const string& endpoint_id, Ptr<EndpointChannel> channel) {
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::OnEncryptionFailureRunnable<Platform>(
          self_, endpoint_id, channel)));
}

template <typename Platform>
void BasePCPHandler<Platform>::runOnPCPHandlerThread(Ptr<Runnable> runnable) {
  serial_executor_->execute(runnable);
}

template <typename Platform>
Ptr<AdvertisingOptions> BasePCPHandler<Platform>::getAdvertisingOptions() {
  return advertising_options_;
}

template <typename Platform>
void BasePCPHandler<Platform>::onEndpointFound(
    Ptr<ClientProxy<Platform>> client_proxy,
    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> endpoint) {
  ScopedPtr<Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>>
      scoped_endpoint(endpoint);

  // Check if we've seen this endpoint ID before.
  Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>
      previously_discovered_endpoint =
          getDiscoveredEndpoint(scoped_endpoint->getEndpointId());

  if (previously_discovered_endpoint.isNull()) {
    const string endpoint_id = scoped_endpoint->getEndpointId();
    const string service_id = scoped_endpoint->getServiceId();
    const string endpoint_name = scoped_endpoint->getEndpointName();
    const proto::connections::Medium medium = scoped_endpoint->getMedium();

    // If this is the first medium we've discovered this endpoint over, then add
    // it to the map.
    discovered_endpoints_.insert(
        std::make_pair(endpoint_id, scoped_endpoint.release()));

    // And, as it's the first time, report it to the client.
    client_proxy->onEndpointFound(endpoint_id, service_id, endpoint_name,
                                  medium);
  } else if (previously_discovered_endpoint->getEndpointName() !=
             scoped_endpoint->getEndpointName()) {
    // If we've already seen this endpoint before, check if there was a name
    // change. If there was, report the previous endpoint as lost.
    // TODO(tracyzhou): Add logging.
    onEndpointLost(client_proxy, previously_discovered_endpoint);
    onEndpointFound(client_proxy, scoped_endpoint.release());
  } else {
    // Otherwise, we need to see if the medium we discovered the endpoint over
    // this time is better than the medium we originally discovered the endpoint
    // over.
    if (isPreferred(scoped_endpoint.get(), previously_discovered_endpoint)) {
      base_pcp_handler::eraseOwnedPtrFromMap(discovered_endpoints_,
                                             scoped_endpoint->getEndpointId());
      discovered_endpoints_.insert(std::make_pair(
          scoped_endpoint->getEndpointId(), scoped_endpoint.release()));
    }
  }
}

template <typename Platform>
void BasePCPHandler<Platform>::onEndpointLost(
    Ptr<ClientProxy<Platform>> client_proxy,
    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> endpoint) {
  ScopedPtr<Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>>
      scoped_endpoint(endpoint);

  // Look up the DiscoveredEndpoint we have in our cache.
  Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>
      discoveredEndpoint =
          getDiscoveredEndpoint(scoped_endpoint->getEndpointId());
  if (discoveredEndpoint.isNull()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  // Validate that the cached endpoint has the same name as the one reported as
  // onLost. If the name differs, then no-op. This likely means that the remote
  // device changed their name. We reported onFound for the new name and are
  // just now figuring out that we lost the old name.
  if (discoveredEndpoint->getEndpointName() !=
      scoped_endpoint->getEndpointName()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  base_pcp_handler::eraseOwnedPtrFromMap(discovered_endpoints_,
                                         scoped_endpoint->getEndpointId());
  client_proxy->onEndpointLost(scoped_endpoint->getServiceId(),
                               scoped_endpoint->getEndpointId());
}

template <typename Platform>
bool BasePCPHandler<Platform>::hasOutgoingConnections(
    Ptr<ClientProxy<Platform>> client_proxy) {
  for (typename PendingConnectionsMap::iterator it =
           pending_connections_.begin();
       it != pending_connections_.end(); it++) {
    if (!it->second->is_incoming_) {
      return true;
    }
  }
  return client_proxy->getNumOutgoingConnections() > 0;
}

template <typename Platform>
bool BasePCPHandler<Platform>::hasIncomingConnections(
    Ptr<ClientProxy<Platform>> client_proxy) {
  for (typename PendingConnectionsMap::iterator it =
           pending_connections_.begin();
       it != pending_connections_.end(); it++) {
    if (it->second->is_incoming_) {
      return true;
    }
  }
  return client_proxy->getNumIncomingConnections() > 0;
}

template <typename Platform>
bool BasePCPHandler<Platform>::canSendOutgoingConnection(
    Ptr<ClientProxy<Platform>> client_proxy) {
  return true;
}

template <typename Platform>
bool BasePCPHandler<Platform>::canReceiveIncomingConnection(
    Ptr<ClientProxy<Platform>> client_proxy) {
  return true;
}

template <typename Platform>
Exception::Value BasePCPHandler<Platform>::writeConnectionRequestFrame(
    Ptr<EndpointChannel> endpoint_channel, const string& local_endpoint_id,
    const string& local_endpoint_name, std::int32_t nonce,
    const std::vector<proto::connections::Medium>& supported_mediums) {
  Exception::Value write_exception =
      endpoint_channel->write(OfflineFrames::forConnectionRequest(
          local_endpoint_id, local_endpoint_name, nonce, supported_mediums));
  if (Exception::NONE != write_exception) {
    if (Exception::IO == write_exception) {
      return write_exception;
    }
  }

  return Exception::NONE;
}

template <typename Platform>
template <typename T>
Ptr<Future<T>> BasePCPHandler<Platform>::runOnPCPHandlerThread(
    Ptr<Callable<T>> callable) {
  return serial_executor_->submit(callable);
}

template <typename Platform>
void BasePCPHandler<Platform>::onConnectionResponse(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    ConstPtr<OfflineFrame> connection_response_offline_frame) {
  ScopedPtr<Ptr<CountDownLatch>> latch(Platform::createCountDownLatch(1));
  runOnPCPHandlerThread(
      MakePtr(new base_pcp_handler::OnConnectionResponseRunnable<Platform>(
          self_, client_proxy, endpoint_id, connection_response_offline_frame,
          latch.get())));
  waitForLatch("onConnectionResponse()", latch.get());
}

template <typename Platform>
bool BasePCPHandler<Platform>::isPreferred(
    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> new_endpoint,
    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> old_endpoint) {
  std::vector<proto::connections::Medium> mediums =
      getConnectionMediumsByPriority();
  // As we iterate through the list of mediums, we see if we run into the new
  // endpoint's medium or the old endpoint's medium first.
  for (std::vector<proto::connections::Medium>::const_iterator it =
           mediums.begin();
       it != mediums.end(); it++) {
    const proto::connections::Medium& medium = *it;
    if (medium == new_endpoint->getMedium()) {
      // The new endpoint's medium came first. It's preferred!
      return true;
    }

    if (medium == old_endpoint->getMedium()) {
      // The old endpoint's medium came first. Stick with the old endpoint!
      return false;
    }
  }
  // TODO(tracyzhou): Add logging.
  assert(false);
  return false;
}

template <typename Platform>
bool BasePCPHandler<Platform>::shouldEnforceTopologyConstraints() {
  // Topology constraints only matter for the advertiser.
  // For discoverers, we'll always enforce them.
  if (getAdvertisingOptions().isNull()) {
    return true;
  }

  return getAdvertisingOptions()->enforce_topology_constraints;
}

template <typename Platform>
bool BasePCPHandler<Platform>::autoUpgradeBandwidth() {
  if (getAdvertisingOptions().isNull()) {
    return true;
  }

  return getAdvertisingOptions()->auto_upgrade_bandwidth;
}

template <typename Platform>
Exception::Value BasePCPHandler<Platform>::onIncomingConnection(
    Ptr<ClientProxy<Platform>> client_proxy, const string& remote_device_name,
    Ptr<EndpointChannel> endpoint_channel, proto::connections::Medium medium) {
  ScopedPtr<Ptr<EndpointChannel>> scoped_endpoint_channel(endpoint_channel);

  std::int64_t start_time_millis = system_clock_->elapsedRealtime();

  //  Fixes an NPE in ClientProxy.onConnectionResult. The crash happened when
  //  the client stopped advertising and we nulled out state, followed by an
  //  incoming connection where we attempted to check that state.
  if (!client_proxy->isAdvertising()) {
    NEARBY_LOG(WARNING,
               "Ignoring incoming connection because client %" PRId64
               " is no longer advertising.",
               client_proxy->getClientId());
    return Exception::IO;
  }

  // Endpoints connecting to us will always tell us about themselves first.
  ExceptionOr<ConstPtr<OfflineFrame>> read_offline_frame =
      readConnectionRequestFrame(scoped_endpoint_channel.get());

  if (!read_offline_frame.ok()) {
    if (Exception::IO == read_offline_frame.exception()) {
      // TODO(tracyzhou): Add logging.
      processPreConnectionInitiationFailure(
          client_proxy, medium, "", scoped_endpoint_channel.get(),
          /* is_incoming= */ true, start_time_millis, Status::ERROR,
          Ptr<SettableFuture<Status::Value>>());
      return Exception::NONE;
    }
  }

  // TODO(tracyzhou): Add logging.
  ScopedPtr<ConstPtr<OfflineFrame>> scoped_read_offline_frame(
      read_offline_frame.result());

  const ConnectionRequestFrame& connection_request =
      scoped_read_offline_frame->v1().connection_request();
  if (client_proxy->isConnectedToEndpoint(connection_request.endpoint_id())) {
    return Exception::IO;
  }

  // If we've already sent out a connection request to this endpoint, then this
  // is where we need to decide which connection to break.
  if (breakTie(client_proxy, connection_request.endpoint_id(),
               connection_request.nonce(), scoped_endpoint_channel.get())) {
    return Exception::NONE;
  }

  // If our child class says we can't accept any more incoming connections,
  // listen to them.
  if (shouldEnforceTopologyConstraints() &&
      !canReceiveIncomingConnection(client_proxy)) {
    return Exception::IO;
  }

  // The ConnectionRequest frame has two fields that both contain the
  // EndpointInfo. The legacy field stores it as a string while the newer field
  // stores it as a byte array. We'll attempt to grab from the newer field, but
  // will accept the older string if it's all that exists.
  const std::string& endpoint_name = connection_request.has_endpoint_info()
                                         ? connection_request.endpoint_info()
                                         : connection_request.endpoint_name();

  // We've successfully connected to the device, and are now about to jump on to
  // the EncryptionRunner thread to start running our encryption protocol. We'll
  // mark ourselves as pending in case we get another call to requestConnection
  // or onIncomingConnection, so that we can cancel the connection if needed.
  endpoint_channel =
      pending_connections_
          .insert(std::make_pair(
              connection_request.endpoint_id(),
              PendingConnectionInfo::newIncomingPendingConnectionInfo(
                  client_proxy, endpoint_name,
                  scoped_endpoint_channel.release(), connection_request.nonce(),
                  start_time_millis, advertising_connection_lifecycle_listener_,
                  OfflineFrames::connectionRequestMediumsToMediums(
                      connection_request))))
          .first->second->endpoint_channel_.get();

  // Next, we'll set up encryption.
  encryption_runner_->startServer(
      client_proxy, connection_request.endpoint_id(), endpoint_channel,
      MakePtr(new
              typename BasePCPHandler<Platform>::ResultListenerFacade(self_)));
  return Exception::NONE;
}

template <typename Platform>
bool BasePCPHandler<Platform>::breakTie(Ptr<ClientProxy<Platform>> client_proxy,
                                        const string& endpoint_id,
                                        std::int32_t incoming_nonce,
                                        Ptr<EndpointChannel> endpoint_channel) {
  typename PendingConnectionsMap::iterator it =
      pending_connections_.find(endpoint_id);
  if (it != pending_connections_.end()) {
    Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
        pending_connection_info = it->second;

    // TODO(tracyzhou): Add logging.

    // Break the lowest connection. In the (extremely) rare case of a tie, break
    // both.
    if (pending_connection_info->nonce_ > incoming_nonce) {
      // Our connection won! Clean up their connection.
      endpoint_channel->close();

      // TODO(tracyzhou): Add logging.
      return true;
    } else if (pending_connection_info->nonce_ < incoming_nonce) {
      // Aw, we lost. Clean up our connection, and then we'll let their
      // connection continue on.
      processTieBreakLoss(client_proxy, endpoint_id, pending_connection_info);

      // TODO(tracyzhou): Add logging.
    } else {
      // Oh. Huh. We both lost. Well, that's awkward. We'll clean up both and
      // just force the devices to retry.
      endpoint_channel->close();

      processTieBreakLoss(client_proxy, endpoint_id, pending_connection_info);

      // TODO(tracyzhou): Add logging.
      return true;
    }
  }

  return false;
}

template <typename Platform>
void BasePCPHandler<Platform>::processTieBreakLoss(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    Ptr<typename BasePCPHandler::PendingConnectionInfo> connection_info) {
  processPreConnectionInitiationFailure(
      client_proxy, connection_info->endpoint_channel_->getMedium(),
      endpoint_id, connection_info->endpoint_channel_.get(),
      connection_info->is_incoming_, connection_info->start_time_millis_,
      Status::ENDPOINT_IO_ERROR, connection_info->request_connection_result_);
  connection_info->request_connection_result_.clear();
  processPreConnectionResultFailure(client_proxy, endpoint_id);
}

template <typename Platform>
void BasePCPHandler<Platform>::initiateBandwidthUpgrade(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    const std::vector<proto::connections::Medium>& supported_mediums) {
  // When we successfully connect to a remote endpoint and a bandwidth upgrade
  // medium has not yet been decided, we'll pick the highest bandwidth medium
  // supported by both us and the remote endpoint. Once we pick a medium, all
  // future connections will use it too. eg. If we chose Wifi LAN, we'll attempt
  // to upgrade the 2nd, 3rd, etc remote endpoints with Wifi LAN even if they're
  // on a different network (or had a better medium). This is a quick and easy
  // way to prevent mediums, like Wifi Hotspot, from interfering with active
  // connections (although it's suboptimal for bandwidth throughput). When all
  // endpoints disconnect, we reset the bandwidth upgrade medium.
  if (bandwidth_upgrade_medium_->get() ==
      proto::connections::Medium::UNKNOWN_MEDIUM) {
    bandwidth_upgrade_medium_->set(chooseBestUpgradeMedium(supported_mediums));
  }

  if (autoUpgradeBandwidth() && (bandwidth_upgrade_medium_->get() !=
                                 proto::connections::Medium::UNKNOWN_MEDIUM)) {
    bandwidth_upgrade_manager_->initiateBandwidthUpgradeForEndpoint(
        client_proxy, endpoint_id, bandwidth_upgrade_medium_->get());
  }
}

template <typename Platform>
proto::connections::Medium BasePCPHandler<Platform>::chooseBestUpgradeMedium(
    const std::vector<proto::connections::Medium>& their_supported_mediums) {
  // If the remote side did not report their supported mediums, choose an
  // appropriate default.
  std::vector<proto::connections::Medium> their_mediums =
      their_supported_mediums;
  if (their_supported_mediums.empty()) {
    their_mediums.push_back(getDefaultUpgradeMedium());
  }

  // Otherwise, pick the best medium we support.
  std::vector<proto::connections::Medium> my_mediums =
      getConnectionMediumsByPriority();
  for (std::vector<proto::connections::Medium>::iterator my_medium =
           my_mediums.begin();
       my_medium != my_mediums.end(); my_medium++) {
    for (std::vector<proto::connections::Medium>::iterator their_medium =
             their_mediums.begin();
         their_medium != their_mediums.end(); their_medium++) {
      if (*my_medium == *their_medium) {
        return *my_medium;
      }
    }
  }

  return proto::connections::Medium::UNKNOWN_MEDIUM;
}

template <typename Platform>
void BasePCPHandler<Platform>::processPreConnectionInitiationFailure(
    Ptr<ClientProxy<Platform>> client_proxy, proto::connections::Medium medium,
    const string& endpoint_id, Ptr<EndpointChannel> endpoint_channel,
    bool is_incoming, std::int64_t start_time_millis, Status::Value status,
    Ptr<SettableFuture<Status::Value>> request_connection_result) {
  // Only *remove* this -- as opposed to *destroying* it by invoking
  // eraseOwnedPtrFromMap() -- because if endpoint_channel is non-null, it's
  // owned by the PendingConnectionInfo in pending_connections_, which means
  // destroying the PendingConnectionInfo right now will lead to a dangling
  // pointer access when we invoke endpoint_channel->close() below.
  ScopedPtr<Ptr<PendingConnectionInfo>> failed_pending_connection(
      base_pcp_handler::removeOwnedPtrFromMap(pending_connections_,
                                              endpoint_id));

  if (!endpoint_channel.isNull()) {
    endpoint_channel->close();
  }

  if (!request_connection_result.isNull()) {
    request_connection_result->set(status);
  }
}

template <typename Platform>
void BasePCPHandler<Platform>::processPreConnectionResultFailure(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id) {
  base_pcp_handler::eraseOwnedPtrFromMap(pending_connections_, endpoint_id);
  endpoint_manager_->discardEndpoint(client_proxy, endpoint_id);
  client_proxy->onConnectionResult(endpoint_id, Status::ERROR);
}

template <typename Platform>
Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>
BasePCPHandler<Platform>::getDiscoveredEndpoint(const string& endpoint_id) {
  typename DiscoveredEndpointsMap::iterator it =
      discovered_endpoints_.find(endpoint_id);
  if (it == discovered_endpoints_.end()) {
    return Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint>();
  }
  return it->second;
}

template <typename Platform>
void BasePCPHandler<Platform>::evaluateConnectionResult(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    bool can_close_immediately) {
  // Short-circuit immediately if we're not in an actionable state yet. We will
  // be called again once the other side has made their decision.
  if (!client_proxy->isConnectionAccepted(endpoint_id) &&
      !client_proxy->isConnectionRejected(endpoint_id)) {
    if (!client_proxy->hasLocalEndpointResponded(endpoint_id)) {
      // TODO(tracyzhou): Add logging.
    } else if (!client_proxy->hasRemoteEndpointResponded(endpoint_id)) {
      // TODO(tracyzhou): Add logging.
    }
    return;
  }

  // Clean up the endpoint channel from our list of 'pending' connections. It's
  // no longer pending.
  typename PendingConnectionsMap::iterator it =
      pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  ScopedPtr<Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>>
      connection_info(it->second);
  pending_connections_.erase(it);

  bool is_connection_accepted = client_proxy->isConnectionAccepted(endpoint_id);

  Status::Value response_code;
  if (is_connection_accepted) {
    // TODO(tracyzhou): Add logging.
    response_code = Status::SUCCESS;

    // Both sides have accepted, so we can now start talking over encrypted
    // channels
    std::unique_ptr<securegcm::D2DConnectionContextV1> encryption_context =
        connection_info->ukey2_handshake_->ToConnectionContext();
    // Java code throws an HandshakeException.
    if (encryption_context == nullptr) {
      // TODO(tracyzhou): Add logging.
      processPreConnectionResultFailure(client_proxy, endpoint_id);
      return;
    }

    endpoint_channel_manager_->encryptChannelForEndpoint(
        endpoint_id, MakeRefCountedPtr(encryption_context.release()));
  } else {
    // TODO(tracyzhou): Add logging.
    response_code = Status::CONNECTION_REJECTED;
  }

  // Invoke the client callback to let it know of the connection result.
  client_proxy->onConnectionResult(endpoint_id, response_code);

  // If the connection failed, clean everything up and short circuit.
  if (!is_connection_accepted) {
    // Clean up the channel in EndpointManager if it's no longer required.
    if (can_close_immediately) {
      endpoint_manager_->discardEndpoint(client_proxy, endpoint_id);
    } else {
      pending_rejected_connection_close_alarms_.insert(std::make_pair(
          endpoint_id,
          MakePtr(new CancelableAlarm(
              "BasePCPHandler.evaluateConnectionResult() delayed close",
              MakePtr(
                  new base_pcp_handler::
                      EvaluateConnectionResultCancelableAlarmRunnable<Platform>(
                          endpoint_manager_, client_proxy, endpoint_id)),
              kRejectedConnectionCloseDelayMillis, alarm_executor_.get()))));
    }

    return;
  }

  // Kick off the bandwidth upgrade for incoming connections.
  if (connection_info->is_incoming_) {
    initiateBandwidthUpgrade(client_proxy, endpoint_id,
                             connection_info->supported_mediums_);
  }
}

template <typename Platform>
ExceptionOr<ConstPtr<OfflineFrame>>
BasePCPHandler<Platform>::readConnectionRequestFrame(
    Ptr<EndpointChannel> endpoint_channel) {
  if (endpoint_channel.isNull()) {
    return ExceptionOr<ConstPtr<OfflineFrame>>(Exception::IO);
  }

  // To avoid a device connecting but never sending their introductory frame, we
  // time out the connection after a certain amount of time.
  CancelableAlarm timeout_alarm(
      "PCPHandler(" + this->getStrategy().getName() +
          ").readConnectionRequestFrame",
      MakePtr(
          new base_pcp_handler::ReadConnectionRequestCancelableAlarmRunnable(
              endpoint_channel)),
      kConnectionRequestReadTimeoutMillis, alarm_executor_.get());

  // Do a blocking read to try and find the ConnectionRequestFrame
  ExceptionOr<ConstPtr<ByteArray>> read_bytes = endpoint_channel->read();
  if (!read_bytes.ok()) {
    if (Exception::IO == read_bytes.exception()) {
      timeout_alarm.cancel();
      return ExceptionOr<ConstPtr<OfflineFrame>>(read_bytes.exception());
    }
  }

  ScopedPtr<ConstPtr<ByteArray>> scoped_read_bytes(read_bytes.result());
  ExceptionOr<ConstPtr<OfflineFrame>> offline_frame =
      OfflineFrames::fromBytes(scoped_read_bytes.get());
  if (!offline_frame.ok()) {
    if (Exception::INVALID_PROTOCOL_BUFFER == offline_frame.exception()) {
      timeout_alarm.cancel();
      // In Java code, INVALID_PROTOCOL_BUFFER is a subtype of IO exception.
      return ExceptionOr<ConstPtr<OfflineFrame>>(Exception::IO);
    }
  }
  timeout_alarm.cancel();

  ScopedPtr<ConstPtr<OfflineFrame>> scoped_offline_frame(
      offline_frame.result());
  if (V1Frame::CONNECTION_REQUEST !=
      OfflineFrames::getFrameType(scoped_offline_frame.get())) {
    return ExceptionOr<ConstPtr<OfflineFrame>>(Exception::IO);
  }

  return ExceptionOr<ConstPtr<OfflineFrame>>(scoped_offline_frame.release());
}

template <typename Platform>
void BasePCPHandler<Platform>::waitForLatch(const string& method_name,
                                            Ptr<CountDownLatch> latch) {
  Exception::Value await_exception = latch->await();
  if (Exception::NONE != await_exception) {
    if (Exception::INTERRUPTED == await_exception) {
      // TODO(tracyzhou): Add logging.
      // Thread.currentThread().interrupt();
    }
  }
}

template <typename Platform>
Status::Value BasePCPHandler<Platform>::waitForResult(
    const string& method_name, std::int64_t client_id,
    Ptr<Future<Status::Value>> result_future) {
  ExceptionOr<Status::Value> result = result_future->get();
  if (!result.ok()) {
    Exception::Value exception = result.exception();
    if (Exception::INTERRUPTED == exception ||
        Exception::EXECUTION == exception) {
      // TODO(tracyzhou): Add logging.
      if (Exception::INTERRUPTED == exception) {
        // Thread.currentThread().interrupt();
      }
      return Status::ERROR;
    }
  }
  return result.result();
}

///////////////////// BasePCPHandler::PendingConnectionInfo ///////////////////

template <typename Platform>
Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
BasePCPHandler<Platform>::PendingConnectionInfo::
    newIncomingPendingConnectionInfo(
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& remote_endpoint_name,
        Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce,
        std::int64_t start_time_millis,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
        const std::vector<proto::connections::Medium>& supported_mediums) {
  return MakePtr(new PendingConnectionInfo(
      client_proxy, remote_endpoint_name, endpoint_channel, nonce, true,
      start_time_millis, connection_lifecycle_listener,
      Ptr<SettableFuture<Status::Value>>(), supported_mediums));
}

template <typename Platform>
Ptr<typename BasePCPHandler<Platform>::PendingConnectionInfo>
BasePCPHandler<Platform>::PendingConnectionInfo::
    newOutgoingPendingConnectionInfo(
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& remote_endpoint_name,
        Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce,
        std::int64_t start_time_millis,
        Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
        Ptr<SettableFuture<Status::Value>> request_connection_result) {
  return MakePtr(new PendingConnectionInfo(
      client_proxy, remote_endpoint_name, endpoint_channel, nonce, false,
      start_time_millis, connection_lifecycle_listener,
      request_connection_result, std::vector<proto::connections::Medium>()));
}

template <typename Platform>
BasePCPHandler<Platform>::PendingConnectionInfo::PendingConnectionInfo(
    Ptr<ClientProxy<Platform>> client_proxy, const string& remote_endpoint_name,
    Ptr<EndpointChannel> endpoint_channel, std::int32_t nonce, bool is_incoming,
    std::int64_t start_time_millis,
    Ptr<ConnectionLifecycleListener> connection_lifecycle_listener,
    Ptr<SettableFuture<Status::Value>> request_connection_result,
    const std::vector<proto::connections::Medium>& supported_mediums)
    : client_proxy_(client_proxy),
      remote_endpoint_name_(remote_endpoint_name),
      endpoint_channel_(endpoint_channel),
      nonce_(nonce),
      is_incoming_(is_incoming),
      start_time_millis_(start_time_millis),
      connection_lifecycle_listener_(connection_lifecycle_listener),
      request_connection_result_(request_connection_result),
      supported_mediums_(supported_mediums),
      ukey2_handshake_() {}

template <typename Platform>
BasePCPHandler<Platform>::PendingConnectionInfo::~PendingConnectionInfo() {
  if (!request_connection_result_.isNull()) {
    request_connection_result_->set(Status::ERROR);
  }

  if (!endpoint_channel_.isNull()) {
    endpoint_channel_->close(proto::connections::DisconnectionReason::SHUTDOWN);
  }

  // Done with operational cleanup, now deallocate memory as needed.
  ukey2_handshake_.destroy();
}

template <typename Platform>
void BasePCPHandler<Platform>::PendingConnectionInfo::setUKey2Handshake(
    Ptr<securegcm::UKey2Handshake> ukey2_handshake) {
  this->ukey2_handshake_ = ukey2_handshake;
}

template <typename Platform>
void BasePCPHandler<Platform>::PendingConnectionInfo::
    localEndpointAcceptedConnection(const string& endpoint_id,
                                    Ptr<PayloadListener> payload_listener) {
  if (!ukey2_handshake_->VerifyHandshake()) {
    NEARBY_LOG(
        FATAL,
        "Failed to verify UKEY2 handshake with %s after accepting locally.",
        endpoint_id.c_str());
  }

  client_proxy_->localEndpointAcceptedConnection(endpoint_id, payload_listener);
}

template <typename Platform>
void BasePCPHandler<Platform>::PendingConnectionInfo::
    localEndpointRejectedConnection(const string& endpoint_id) {
  client_proxy_->localEndpointRejectedConnection(endpoint_id);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
