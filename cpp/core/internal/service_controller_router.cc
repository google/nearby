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

#include "core/internal/service_controller_router.h"

#include "core/internal/offline_service_controller.h"

namespace location {
namespace nearby {
namespace connections {

namespace service_controller_router {

// Base class for the following Runnable classes. They all need a
// ServiceControllerRouter object and a ClientProxy object.
// ServiceControllerRouter is kept as a reference because the passed in
// Ptr<ServiceControllerRouter<Platform> > should outlive it.
template <typename Platform>
class ServiceControllerRouterRunnable : public Runnable {
 protected:
  ServiceControllerRouterRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy)
      : service_controller_router_(service_controller_router),
        client_proxy_(client_proxy) {}

  Ptr<ServiceControllerRouter<Platform> > service_controller_router_;
  Ptr<ClientProxy<Platform> > client_proxy_;
};

template <typename Platform>
class StartAdvertisingRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  StartAdvertisingRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StartAdvertisingParams> start_advertising_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(start_advertising_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    Status::Value status =
        this->service_controller_router_->acquireServiceControllerForClient(
            this->client_proxy_, params_->advertising_options.strategy);
    if (Status::SUCCESS != status) {
      result_listener->onResult(status);
      return;
    }

    if (this->client_proxy_->isAdvertising()) {
      result_listener->onResult(Status::ALREADY_ADVERTISING);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->startAdvertising(this->client_proxy_, params_->name,
                               params_->service_id,
                               params_->advertising_options,
                               params_->connection_lifecycle_listener));
  }

 private:
  ScopedPtr<ConstPtr<StartAdvertisingParams> > params_;
};

template <typename Platform>
class StopAdvertisingRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  StopAdvertisingRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StopAdvertisingParams> stop_advertising_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(stop_advertising_params) {}

  void run() override {
    if (this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_) &&
        this->client_proxy_->isAdvertising()) {
      this->service_controller_router_->current_service_controller_
          ->stopAdvertising(this->client_proxy_);
    }
  }

 private:
  ScopedPtr<ConstPtr<StopAdvertisingParams> > params_;
};

template <typename Platform>
class StartDiscoveryRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  StartDiscoveryRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StartDiscoveryParams> start_discovery_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(start_discovery_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    Status::Value status =
        this->service_controller_router_->acquireServiceControllerForClient(
            this->client_proxy_, params_->discovery_options.strategy);
    if (Status::SUCCESS != status) {
      result_listener->onResult(status);
      return;
    }

    if (this->client_proxy_->isDiscovering()) {
      result_listener->onResult(Status::ALREADY_DISCOVERING);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->startDiscovery(this->client_proxy_, params_->service_id,
                             params_->discovery_options,
                             params_->discovery_listener));
  }

 private:
  ScopedPtr<ConstPtr<StartDiscoveryParams> > params_;
};

template <typename Platform>
class StopDiscoveryRunnable : public ServiceControllerRouterRunnable<Platform> {
 public:
  StopDiscoveryRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StopDiscoveryParams> stop_discovery_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(stop_discovery_params) {}

  void run() override {
    if (this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_) &&
        this->client_proxy_->isDiscovering()) {
      this->service_controller_router_->current_service_controller_
          ->stopDiscovery(this->client_proxy_);
    }
  }

 private:
  ScopedPtr<ConstPtr<StopDiscoveryParams> > params_;
};

template <typename Platform>
class SendConnectionRequestRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  SendConnectionRequestRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<RequestConnectionParams> request_connection_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(request_connection_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    const string& remote_endpoint_id = params_->remote_endpoint_id;

    if (this->client_proxy_->hasPendingConnectionToEndpoint(
            remote_endpoint_id) ||
        this->client_proxy_->isConnectedToEndpoint(remote_endpoint_id)) {
      result_listener->onResult(Status::ALREADY_CONNECTED_TO_ENDPOINT);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->requestConnection(this->client_proxy_, params_->name,
                                remote_endpoint_id,
                                params_->connection_lifecycle_listener));
  }

 private:
  ScopedPtr<ConstPtr<RequestConnectionParams> > params_;
};

template <typename Platform>
class AcceptConnectionRequestRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  AcceptConnectionRequestRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<AcceptConnectionParams> accept_connection_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(accept_connection_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    const string& remote_endpoint_id = params_->remote_endpoint_id;

    if (this->client_proxy_->isConnectedToEndpoint(remote_endpoint_id)) {
      result_listener->onResult(Status::ALREADY_CONNECTED_TO_ENDPOINT);
      return;
    }

    if (this->client_proxy_->hasLocalEndpointResponded(remote_endpoint_id)) {
      // TODO(tracyzhou): logging
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->acceptConnection(this->client_proxy_, remote_endpoint_id,
                               params_->payload_listener));
  }

 private:
  ScopedPtr<ConstPtr<AcceptConnectionParams> > params_;
};

template <typename Platform>
class RejectConnectionRequestRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  RejectConnectionRequestRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<RejectConnectionParams> reject_connection_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(reject_connection_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    const string& remote_endpoint_id = params_->remote_endpoint_id;

    if (this->client_proxy_->isConnectedToEndpoint(remote_endpoint_id)) {
      result_listener->onResult(Status::ALREADY_CONNECTED_TO_ENDPOINT);
      return;
    }

    if (this->client_proxy_->hasLocalEndpointResponded(remote_endpoint_id)) {
      // TODO(tracyzhou): logging
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->rejectConnection(this->client_proxy_, remote_endpoint_id));
  }

 private:
  ScopedPtr<ConstPtr<RejectConnectionParams> > params_;
};

template <typename Platform>
class InitiateBandwidthUpgradeRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  InitiateBandwidthUpgradeRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<InitiateBandwidthUpgradeParams>
          initiate_bandwidth_upgrade_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(initiate_bandwidth_upgrade_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_) ||
        !this->client_proxy_->isConnectedToEndpoint(
            params_->remote_endpoint_id)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    this->service_controller_router_->current_service_controller_
        ->initiateBandwidthUpgrade(this->client_proxy_,
                                   params_->remote_endpoint_id);

    // The caller can listen to
    // ConnectionLifecycleListener.onBandwidthChanged() to determine success.
    result_listener->onResult(Status::SUCCESS);
  }

 private:
  ScopedPtr<ConstPtr<InitiateBandwidthUpgradeParams> > params_;
};

template <typename Platform>
class SendPayloadRunnable : public ServiceControllerRouterRunnable<Platform> {
 public:
  SendPayloadRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<SendPayloadParams> send_payload_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(send_payload_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    if (!ServiceControllerRouter<Platform>::
            clientHasConnectionToAtLeastOneEndpoint(
                this->client_proxy_, params_->remote_endpoint_ids)) {
      result_listener->onResult(Status::ENDPOINT_UNKNOWN);
      return;
    }

    this->service_controller_router_->current_service_controller_->sendPayload(
        this->client_proxy_, params_->remote_endpoint_ids, params_->payload);

    // At this point, we've queued up the send Payload request with the
    // ServiceController; any further failures (e.g. one of the endpoints is
    // unknown, goes away, or otherwise fails) will be returned to the client
    // as a PayloadTransferUpdate.
    result_listener->onResult(Status::SUCCESS);
  }

 private:
  ScopedPtr<ConstPtr<SendPayloadParams> > params_;
};

template <typename Platform>
class CancelPayloadRunnable : public ServiceControllerRouterRunnable<Platform> {
 public:
  CancelPayloadRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<CancelPayloadParams> cancel_payload_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(cancel_payload_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      result_listener->onResult(Status::OUT_OF_ORDER_API_CALL);
      return;
    }

    result_listener->onResult(
        this->service_controller_router_->current_service_controller_
            ->cancelPayload(this->client_proxy_, params_->payload_id));
  }

 private:
  ScopedPtr<ConstPtr<CancelPayloadParams> > params_;
};

template <typename Platform>
class DisconnectFromEndpointRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  DisconnectFromEndpointRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<DisconnectFromEndpointParams> disconnect_from_endpoint_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(disconnect_from_endpoint_params) {}

  void run() override {
    if (this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      const string& remote_endpoint_id = params_->remote_endpoint_id;

      if (!this->client_proxy_->isConnectedToEndpoint(remote_endpoint_id) &&
          !this->client_proxy_->hasPendingConnectionToEndpoint(
              remote_endpoint_id)) {
        return;
      }
      this->service_controller_router_->current_service_controller_
          ->disconnectFromEndpoint(this->client_proxy_, remote_endpoint_id);
    }
  }

 private:
  ScopedPtr<ConstPtr<DisconnectFromEndpointParams> > params_;
};

template <typename Platform>
class StopAllEndpointsRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  StopAllEndpointsRunnable(
      Ptr<ServiceControllerRouter<Platform> > service_controller_router,
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StopAllEndpointsParams> stop_all_endpoints_params)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy),
        params_(stop_all_endpoints_params) {}

  void run() override {
    ScopedPtr<Ptr<ResultListener> > result_listener(params_->result_listener);

    if (this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      this->service_controller_router_->doneWithStrategySessionForClient(
          this->client_proxy_);
    }
    result_listener->onResult(Status::SUCCESS);
  }

 private:
  ScopedPtr<ConstPtr<StopAllEndpointsParams> > params_;
};

template <typename Platform>
class ClientDisconnectingRunnable
    : public ServiceControllerRouterRunnable<Platform> {
 public:
  ClientDisconnectingRunnable(
      Ptr<ServiceControllerRouter<Platform>> service_controller_router,
      Ptr<ClientProxy<Platform>> client_proxy)
      : ServiceControllerRouterRunnable<Platform>(service_controller_router,
                                                  client_proxy) {}

  void run() override {
    if (!this->service_controller_router_->clientHasAquiredServiceController(
            this->client_proxy_)) {
      return;
    }

    this->service_controller_router_->doneWithStrategySessionForClient(
        this->client_proxy_);

    // Log the completion of this client's connection.
    // TODO(tracyzhou): Add logging.
  }
};

}  // namespace service_controller_router

template <typename Platform>
ServiceControllerRouter<Platform>::ServiceControllerRouter()
    : current_service_controller_clients_(),
      current_service_controller_(new OfflineServiceController<Platform>()),
      current_strategy_(),
      serializer_(Platform::createSingleThreadExecutor()) {}

template <typename Platform>
ServiceControllerRouter<Platform>::~ServiceControllerRouter() {
  // TODO(tracyzhou): Add logging.

  // And make sure that cleanup is the last thing we do.
  serializer_->shutdown();

  current_service_controller_.destroy();
  current_strategy_.destroy();
  current_service_controller_clients_.clear();
}

template <typename Platform>
void ServiceControllerRouter<Platform>::startAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<StartAdvertisingParams> start_advertising_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::StartAdvertisingRunnable<Platform>(
          self_, client_proxy, start_advertising_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::stopAdvertising(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<StopAdvertisingParams> stop_advertising_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::StopAdvertisingRunnable<Platform>(
          self_, client_proxy, stop_advertising_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::startDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<StartDiscoveryParams> start_discovery_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::StartDiscoveryRunnable<Platform>(
          self_, client_proxy, start_discovery_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::stopDiscovery(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<StopDiscoveryParams> stop_discovery_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::StopDiscoveryRunnable<Platform>(
          self_, client_proxy, stop_discovery_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::requestConnection(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<RequestConnectionParams> request_connection_params) {
  routeToServiceController(MakePtr(
      new service_controller_router::SendConnectionRequestRunnable<Platform>(
          self_, client_proxy, request_connection_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::acceptConnection(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<AcceptConnectionParams> accept_connection_params) {
  routeToServiceController(MakePtr(
      new service_controller_router::AcceptConnectionRequestRunnable<Platform>(
          self_, client_proxy, accept_connection_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::rejectConnection(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<RejectConnectionParams> reject_connection_params) {
  routeToServiceController(MakePtr(
      new service_controller_router::RejectConnectionRequestRunnable<Platform>(
          self_, client_proxy, reject_connection_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::initiateBandwidthUpgrade(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<InitiateBandwidthUpgradeParams>
        initiate_bandwidth_upgrade_params) {
  routeToServiceController(MakePtr(
      new service_controller_router::InitiateBandwidthUpgradeRunnable<Platform>(
          self_, client_proxy, initiate_bandwidth_upgrade_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::sendPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<SendPayloadParams> send_payload_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::SendPayloadRunnable<Platform>(
          self_, client_proxy, send_payload_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::cancelPayload(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<CancelPayloadParams> cancel_payload_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::CancelPayloadRunnable<Platform>(
          self_, client_proxy, cancel_payload_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::disconnectFromEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<DisconnectFromEndpointParams> disconnect_from_endpoint_params) {
  routeToServiceController(MakePtr(
      new service_controller_router::DisconnectFromEndpointRunnable<Platform>(
          self_, client_proxy, disconnect_from_endpoint_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::stopAllEndpoints(
    Ptr<ClientProxy<Platform> > client_proxy,
    ConstPtr<StopAllEndpointsParams> stop_all_endpoint_params) {
  routeToServiceController(
      MakePtr(new service_controller_router::StopAllEndpointsRunnable<Platform>(
          self_, client_proxy, stop_all_endpoint_params)));
}

template <typename Platform>
void ServiceControllerRouter<Platform>::clientDisconnecting(
    Ptr<ClientProxy<Platform>> client_proxy) {
  routeToServiceController(MakePtr(
      new service_controller_router::ClientDisconnectingRunnable<Platform>(
          self_, client_proxy)));
}

template <typename Platform>
Status::Value
ServiceControllerRouter<Platform>::acquireServiceControllerForClient(
    Ptr<ClientProxy<Platform> > client_proxy, const Strategy& strategy) {
  if (current_strategy_.isNull()) {
    // Case 1: There is no existing Strategy at all.

    // Set everything up for the first time.
    Status::Value status = updateCurrentServiceControllerAndStrategy(strategy);
    if (status != Status::SUCCESS) {
      return status;
    }
    current_service_controller_clients_.insert(client_proxy);
    return Status::SUCCESS;
  } else if (strategy == *current_strategy_) {
    // Case 2: The existing Strategy matches.

    // The new client just needs to be added to the set of clients using the
    // current ServiceController.
    current_service_controller_clients_.insert(client_proxy);
    return Status::SUCCESS;
  } else {
    // Case 3: The existing Strategy doesn't match.

    // It's only safe for a client to cause a switch if it's the only client
    // using the current ServiceController.
    bool is_the_only_client_of_service_controller =
        current_service_controller_clients_.size() == 1 &&
        current_service_controller_clients_.find(client_proxy) !=
            current_service_controller_clients_.end();
    if (!is_the_only_client_of_service_controller) {
      // TODO(tracyzhou): logging
      return Status::ALREADY_HAVE_ACTIVE_STRATEGY;
    }

    // If the client still has connected endpoints, they must disconnect before
    // they can switch.
    if (!client_proxy->getConnectedEndpoints().empty()) {
      // TODO(tracyzhou): logging
      return Status::OUT_OF_ORDER_API_CALL;
    }

    // By this point, it's safe to switch the Strategy and ServiceController
    // (and since it's the only client, there's no need to add it to the set of
    // clients using the current ServiceController).
    return updateCurrentServiceControllerAndStrategy(strategy);
  }
}

template <typename Platform>
bool ServiceControllerRouter<Platform>::clientHasAquiredServiceController(
    Ptr<ClientProxy<Platform> > client_proxy) {
  return (current_service_controller_clients_.find(client_proxy) !=
          current_service_controller_clients_.end());
}

template <typename Platform>
void ServiceControllerRouter<Platform>::releaseServiceControllerForClient(
    Ptr<ClientProxy<Platform> > client_proxy) {
  current_service_controller_clients_.erase(client_proxy);

  if (current_service_controller_clients_.empty()) {
    current_service_controller_.destroy();
    current_strategy_.destroy();
  }
}

/** Clean up all state for this client. The client is now free to switch
 * strategies. */
template <typename Platform>
void ServiceControllerRouter<Platform>::doneWithStrategySessionForClient(
    Ptr<ClientProxy<Platform> > client_proxy) {
  // Disconnect from all the connected endpoints tied to this clientProxy.
  std::vector<string> pending_connected_endpoints =
      client_proxy->getPendingConnectedEndpoints();

  for (std::vector<string>::iterator it = pending_connected_endpoints.begin();
       it != pending_connected_endpoints.end(); it++) {
    current_service_controller_->disconnectFromEndpoint(client_proxy, *it);
  }

  std::vector<string> connected_endpoints =
      client_proxy->getConnectedEndpoints();

  for (std::vector<string>::iterator it = connected_endpoints.begin();
       it != connected_endpoints.end(); it++) {
    current_service_controller_->disconnectFromEndpoint(client_proxy, *it);
  }

  // Stop any advertising and discovery that may be underway due to this
  // clientProxy.
  current_service_controller_->stopAdvertising(client_proxy);
  current_service_controller_->stopDiscovery(client_proxy);

  // Finally, clear all state maintained by this clientProxy.
  client_proxy->reset();

  releaseServiceControllerForClient(client_proxy);
}

template <typename Platform>
void ServiceControllerRouter<Platform>::routeToServiceController(
    Ptr<Runnable> runnable) {
  serializer_->execute(runnable);
}

template <typename Platform>
bool ServiceControllerRouter<Platform>::clientHasConnectionToAtLeastOneEndpoint(
    Ptr<ClientProxy<Platform> > client_proxy,
    const std::vector<string>& remote_endpoint_ids) {
  for (std::vector<string>::const_iterator it = remote_endpoint_ids.begin();
       it != remote_endpoint_ids.end(); it++) {
    if (client_proxy->isConnectedToEndpoint(*it)) {
      return true;
    }
  }
  return false;
}

template <typename Platform>
Status::Value
ServiceControllerRouter<Platform>::updateCurrentServiceControllerAndStrategy(
    const Strategy& strategy) {
  if (!strategy.isValid()) {
    // TODO(tracyzhou): logging
    return Status::ERROR;
  }

  current_service_controller_.destroy();
  current_service_controller_ =
      MakePtr(new OfflineServiceController<Platform>());
  current_strategy_.destroy();
  current_strategy_ = MakePtr(new Strategy(strategy));

  return Status::SUCCESS;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
