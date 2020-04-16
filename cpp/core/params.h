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

#ifndef CORE_PARAMS_H_
#define CORE_PARAMS_H_

#include <cstdint>
#include <vector>

#include "core/listeners.h"
#include "core/options.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

struct StartAdvertisingParams {
  Ptr<ResultListener> result_listener;
  const std::string name;
  const std::string service_id;
  const AdvertisingOptions advertising_options;
  Ptr<ConnectionLifecycleListener> connection_lifecycle_listener;

  StartAdvertisingParams(
      Ptr<ResultListener> result_listener, const std::string& name,
      const std::string& service_id,
      const AdvertisingOptions& advertising_options,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener)
      : result_listener(result_listener),
        name(name),
        service_id(service_id),
        advertising_options(advertising_options),
        connection_lifecycle_listener(connection_lifecycle_listener) {}
};

struct StopAdvertisingParams {
  // Intentionally left empty.
};

struct StartDiscoveryParams {
  Ptr<ResultListener> result_listener;
  const std::string service_id;
  const DiscoveryOptions discovery_options;
  Ptr<DiscoveryListener> discovery_listener;

  StartDiscoveryParams(Ptr<ResultListener> result_listener,
                       const std::string& service_id,
                       const DiscoveryOptions& discovery_options,
                       Ptr<DiscoveryListener> discovery_listener)
      : result_listener(result_listener),
        service_id(service_id),
        discovery_options(discovery_options),
        discovery_listener(discovery_listener) {}
};

struct StopDiscoveryParams {
  // Intentionally left empty.
};

struct RequestConnectionParams {
  Ptr<ResultListener> result_listener;
  const std::string name;
  const std::string remote_endpoint_id;
  Ptr<ConnectionLifecycleListener> connection_lifecycle_listener;

  RequestConnectionParams(
      Ptr<ResultListener> result_listener, const std::string& name,
      const std::string& remote_endpoint_id,
      Ptr<ConnectionLifecycleListener> connection_lifecycle_listener)
      : result_listener(result_listener),
        name(name),
        remote_endpoint_id(remote_endpoint_id),
        connection_lifecycle_listener(connection_lifecycle_listener) {}
};

struct AcceptConnectionParams {
  Ptr<ResultListener> result_listener;
  const std::string remote_endpoint_id;
  Ptr<PayloadListener> payload_listener;

  AcceptConnectionParams(Ptr<ResultListener> result_listener,
                         const std::string& remote_endpoint_id,
                         Ptr<PayloadListener> payload_listener)
      : result_listener(result_listener),
        remote_endpoint_id(remote_endpoint_id),
        payload_listener(payload_listener) {}
};

struct RejectConnectionParams {
  Ptr<ResultListener> result_listener;
  const std::string remote_endpoint_id;

  RejectConnectionParams(Ptr<ResultListener> result_listener,
                         const std::string& remote_endpoint_id)
      : result_listener(result_listener),
        remote_endpoint_id(remote_endpoint_id) {}
};

struct SendPayloadParams {
  Ptr<ResultListener> result_listener;
  const std::vector<std::string> remote_endpoint_ids;
  ConstPtr<Payload> payload;

  SendPayloadParams(Ptr<ResultListener> result_listener,
                    const std::vector<std::string>& remote_endpoint_ids,
                    ConstPtr<Payload> payload)
      : result_listener(result_listener),
        remote_endpoint_ids(remote_endpoint_ids),
        payload(payload) {}
};

struct CancelPayloadParams {
  Ptr<ResultListener> result_listener;
  const std::int64_t payload_id;

  CancelPayloadParams(Ptr<ResultListener> result_listener,
                      std::int64_t payload_id)
      : result_listener(result_listener), payload_id(payload_id) {}
};

struct InitiateBandwidthUpgradeParams {
  Ptr<ResultListener> result_listener;
  const std::string remote_endpoint_id;

  InitiateBandwidthUpgradeParams(Ptr<ResultListener> result_listener,
                                 const std::string& remote_endpoint_id)
      : result_listener(result_listener),
        remote_endpoint_id(remote_endpoint_id) {}
};

struct DisconnectFromEndpointParams {
  const std::string remote_endpoint_id;

  explicit DisconnectFromEndpointParams(const std::string& remote_endpoint_id)
      : remote_endpoint_id(remote_endpoint_id) {}
};

struct StopAllEndpointsParams {
  Ptr<ResultListener> result_listener;

  explicit StopAllEndpointsParams(Ptr<ResultListener> result_listener)
      : result_listener(result_listener) {}
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_PARAMS_H_
