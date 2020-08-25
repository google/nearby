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

#ifndef CORE_V2_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_
#define CORE_V2_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_

#include <memory>
#include <string>
#include <vector>

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/service_controller.h"
#include "core_v2/options.h"
#include "core_v2/params.h"
#include "platform_v2/base/runnable.h"
#include "platform_v2/public/single_thread_executor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {

// ServiceControllerRouter: this class is an implementation detail of a
// location::nearby::Core class. The latter delegates all of its activities to
// the former.
//
// All the activities are documented in the public API class:
// cpp/core_v2/core.h
//
// In every method, ClientProxy* represents the client app which receives
// notifications from Nearby Connections service and forwards them to the app.
// The rest of arguments have the same meaning as the corresponding
// methods in the definition of location::nearby::Core API.
//
// Every activity is handled the same way:
// 1) all the arguments to the call are captured by value;
// 2) the actual processing is scheduled on a private single-threaded executor,
//    which makes locking unnecessary, when internal data is being manipulated.
// 3) activity handlers are delegating much of their work to an implementation
//    of a ServiceController interface, which does the actual job.
class ServiceControllerRouter {
 public:
  explicit ServiceControllerRouter(std::function<ServiceController*()> factory)
      : service_controller_factory_(std::move(factory)) {}
  ~ServiceControllerRouter();
  ServiceControllerRouter(ServiceControllerRouter&&) = default;
  ServiceControllerRouter& operator=(ServiceControllerRouter&&) = default;

  void StartAdvertising(ClientProxy* client, absl::string_view service_id,
                        const ConnectionOptions& options,
                        const ConnectionRequestInfo& info,
                        const ResultCallback& callback);
  void StopAdvertising(ClientProxy* client, const ResultCallback& callback);

  void StartDiscovery(ClientProxy* client, absl::string_view service_id,
                      const ConnectionOptions& options,
                      const DiscoveryListener& listener,
                      const ResultCallback& callback);
  void StopDiscovery(ClientProxy* client, const ResultCallback& callback);

  void RequestConnection(ClientProxy* client, absl::string_view endpoint_id,
                         const ConnectionRequestInfo& info,
                         const ConnectionOptions& options,
                         const ResultCallback& callback);
  void AcceptConnection(ClientProxy* client, absl::string_view endpoint_id,
                        const PayloadListener& listener,
                        const ResultCallback& callback);
  void RejectConnection(ClientProxy* client, absl::string_view endpoint_id,
                        const ResultCallback& callback);

  void InitiateBandwidthUpgrade(ClientProxy* client,
                                absl::string_view endpoint_id,
                                const ResultCallback& callback);

  void SendPayload(ClientProxy* client,
                   absl::Span<const std::string> endpoint_ids, Payload payload,
                   const ResultCallback& callback);
  void CancelPayload(ClientProxy* client, std::uint64_t payload_id,
                     const ResultCallback& callback);

  void DisconnectFromEndpoint(ClientProxy* client,
                              absl::string_view endpoint_id,
                              const ResultCallback& callback);
  void StopAllEndpoints(ClientProxy* client, const ResultCallback& callback);

  void ClientDisconnecting(ClientProxy* client, const ResultCallback& callback);

 private:
  friend class ServiceControllerRouterTest;
  static bool ClientHasConnectionToAtLeastOneEndpoint(
      ClientProxy* client, const std::vector<std::string>& remote_endpoint_ids);

  void RouteToServiceController(Runnable runnable);

  Status AcquireServiceControllerForClient(ClientProxy* client,
                                           Strategy strategy);
  bool ClientHasAcquiredServiceController(ClientProxy* client) const;
  void ReleaseServiceControllerForClient(ClientProxy* client);
  void DoneWithStrategySessionForClient(ClientProxy* client);
  Status UpdateCurrentServiceControllerAndStrategy(Strategy strategy);

  absl::flat_hash_set<ClientProxy*> clients_;
  std::function<ServiceController*()> service_controller_factory_;
  std::unique_ptr<ServiceController> service_controller_;
  Strategy current_strategy_;
  SingleThreadExecutor serializer_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_
