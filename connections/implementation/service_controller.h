// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_SERVICE_CONTROLLER_H_
#define CORE_INTERNAL_SERVICE_CONTROLLER_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "connections/advertising_options.h"
#include "connections/implementation/client_proxy.h"
#include "connections/listeners.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/listeners.h"
#include "internal/platform/borrowable.h"

namespace nearby {
namespace connections {

// Interface defines the core functionality of Nearby Connections Service.
//
// In every method, ClientProxy* represents the client app which receives
// notifications from Nearby Connections service and forwards them to the app.
// ResultCallback arguments are not provided for this class, because all methods
// are called synchronously.
// The rest of arguments have the same meaning as the corresponding
// methods in the definition of nearby::Core API.
//
// See details here:
// cpp/core/core.h
class ServiceController {
 public:
  virtual ~ServiceController() = default;
  ServiceController() = default;
  ServiceController(const ServiceController&) = delete;
  ServiceController& operator=(const ServiceController&) = delete;

  // Stops and disables service controller.
  //
  // When service controller is stopped all API call fail early.
  // Note that all Core, ClientProxy objects referencing this service
  // controller are affected.
  virtual void Stop() = 0;

  // Shuts down executors in the BwuManager. After that no tasks should be
  // running on or posted to BwuManager.
  virtual void ShutdownBwuManagerExecutors() = 0;

  // Starts advertising an endpoint for a local app.
  virtual Status StartAdvertising(::nearby::Borrowable<ClientProxy*> client,
                                  const std::string& service_id,
                                  const AdvertisingOptions& advertising_options,
                                  const ConnectionRequestInfo& info) = 0;

  virtual void StopAdvertising(::nearby::Borrowable<ClientProxy*> client) = 0;

  virtual Status StartDiscovery(::nearby::Borrowable<ClientProxy*> client,
                                const std::string& service_id,
                                const DiscoveryOptions& discovery_options,
                                const DiscoveryListener& listener) = 0;
  virtual void StopDiscovery(::nearby::Borrowable<ClientProxy*> client) = 0;

  virtual void InjectEndpoint(::nearby::Borrowable<ClientProxy*> client,
                              const std::string& service_id,
                              const OutOfBandConnectionMetadata& metadata) = 0;

  virtual std::pair<Status, std::vector<ConnectionInfoVariant>>
  StartListeningForIncomingConnections(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options) = 0;

  virtual void StopListeningForIncomingConnections(
      ::nearby::Borrowable<ClientProxy*> client) = 0;

  virtual Status RequestConnection(
      ::nearby::Borrowable<ClientProxy*> client, const std::string& endpoint_id,
      const ConnectionRequestInfo& info,
      const ConnectionOptions& connection_options) = 0;
  virtual Status AcceptConnection(::nearby::Borrowable<ClientProxy*> client,
                                  const std::string& endpoint_id,
                                  PayloadListener listener) = 0;
  virtual Status RejectConnection(::nearby::Borrowable<ClientProxy*> client,
                                  const std::string& endpoint_id) = 0;

  virtual void InitiateBandwidthUpgrade(
      ::nearby::Borrowable<ClientProxy*> client,
      const std::string& endpoint_id) = 0;

  virtual void SendPayload(::nearby::Borrowable<ClientProxy*> client,
                           const std::vector<std::string>& endpoint_ids,
                           Payload payload) = 0;

  virtual Status CancelPayload(::nearby::Borrowable<ClientProxy*> client,
                               Payload::Id payload_id) = 0;

  virtual void DisconnectFromEndpoint(::nearby::Borrowable<ClientProxy*> client,
                                      const std::string& endpoint_id) = 0;

  virtual Status UpdateAdvertisingOptions(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const AdvertisingOptions& advertising_options) = 0;

  virtual Status UpdateDiscoveryOptions(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const DiscoveryOptions& discovery_options) = 0;

  virtual void SetCustomSavePath(::nearby::Borrowable<ClientProxy*> client,
                                 const std::string& path) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_SERVICE_CONTROLLER_H_
