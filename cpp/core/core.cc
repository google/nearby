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

#include "core/core.h"

#include <cassert>

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
Core<Platform>::Core()
    : client_proxy_(new ClientProxy<Platform>()),
      service_controller_router_(new ServiceControllerRouter<Platform>()) {}

template <typename Platform>
Core<Platform>::~Core() {
  service_controller_router_->clientDisconnecting(client_proxy_.get());
}

template <typename Platform>
void Core<Platform>::startAdvertising(
    ConstPtr<StartAdvertisingParams> start_advertising_params) {
  assert(!start_advertising_params->result_listener.isNull());
  assert(!start_advertising_params->connection_lifecycle_listener.isNull());
  assert(!start_advertising_params->service_id.empty());
  assert(start_advertising_params->advertising_options.strategy.isValid());

  service_controller_router_->startAdvertising(client_proxy_.get(),
                                               start_advertising_params);
}

template <typename Platform>
void Core<Platform>::stopAdvertising(
    ConstPtr<StopAdvertisingParams> stop_advertising_params) {
  service_controller_router_->stopAdvertising(client_proxy_.get(),
                                              stop_advertising_params);
}

template <typename Platform>
void Core<Platform>::startDiscovery(
    ConstPtr<StartDiscoveryParams> start_discovery_params) {
  assert(!start_discovery_params->result_listener.isNull());
  assert(!start_discovery_params->discovery_listener.isNull());
  assert(!start_discovery_params->service_id.empty());
  assert(start_discovery_params->discovery_options.strategy.isValid());

  service_controller_router_->startDiscovery(client_proxy_.get(),
                                             start_discovery_params);
}

template <typename Platform>
void Core<Platform>::stopDiscovery(
    ConstPtr<StopDiscoveryParams> stop_discovery_params) {
  service_controller_router_->stopDiscovery(client_proxy_.get(),
                                            stop_discovery_params);
}

template <typename Platform>
void Core<Platform>::requestConnection(
    ConstPtr<RequestConnectionParams> request_connection_params) {
  assert(!request_connection_params->result_listener.isNull());
  assert(!request_connection_params->connection_lifecycle_listener.isNull());
  assert(!request_connection_params->remote_endpoint_id.empty());

  service_controller_router_->requestConnection(client_proxy_.get(),
                                                request_connection_params);
}

template <typename Platform>
void Core<Platform>::acceptConnection(
    ConstPtr<AcceptConnectionParams> accept_connection_params) {
  assert(!accept_connection_params->result_listener.isNull());
  assert(!accept_connection_params->payload_listener.isNull());
  assert(!accept_connection_params->remote_endpoint_id.empty());

  service_controller_router_->acceptConnection(client_proxy_.get(),
                                               accept_connection_params);
}

template <typename Platform>
void Core<Platform>::rejectConnection(
    ConstPtr<RejectConnectionParams> reject_connection_params) {
  assert(!reject_connection_params->result_listener.isNull());
  assert(!reject_connection_params->remote_endpoint_id.empty());

  service_controller_router_->rejectConnection(client_proxy_.get(),
                                               reject_connection_params);
}

template <typename Platform>
void Core<Platform>::initiateBandwidthUpgrade(
    ConstPtr<InitiateBandwidthUpgradeParams>
        initiate_bandwidth_upgrade_params) {
  service_controller_router_->initiateBandwidthUpgrade(
      client_proxy_.get(), initiate_bandwidth_upgrade_params);
}

template <typename Platform>
void Core<Platform>::sendPayload(
    ConstPtr<SendPayloadParams> send_payload_params) {
  assert(!send_payload_params->result_listener.isNull());
  assert(!send_payload_params->remote_endpoint_ids.empty());
  assert(!send_payload_params->payload.isNull());
  // TODO(tracyzhou): Do sanity check on payload based on payload type.

  service_controller_router_->sendPayload(client_proxy_.get(),
                                          send_payload_params);
}

template <typename Platform>
void Core<Platform>::cancelPayload(
    ConstPtr<CancelPayloadParams> cancel_payload_params) {
  assert(!cancel_payload_params->result_listener.isNull());
  assert(cancel_payload_params->payload_id != 0);

  service_controller_router_->cancelPayload(client_proxy_.get(),
                                            cancel_payload_params);
}

template <typename Platform>
void Core<Platform>::disconnectFromEndpoint(
    ConstPtr<DisconnectFromEndpointParams> disconnect_from_endpoint_params) {
  assert(!disconnect_from_endpoint_params->remote_endpoint_id.empty());

  service_controller_router_->disconnectFromEndpoint(
      client_proxy_.get(), disconnect_from_endpoint_params);
}

template <typename Platform>
void Core<Platform>::stopAllEndpoints(
    ConstPtr<StopAllEndpointsParams> stop_all_endpoints_params) {
  service_controller_router_->stopAllEndpoints(client_proxy_.get(),
                                               stop_all_endpoints_params);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
