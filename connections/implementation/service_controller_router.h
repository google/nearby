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

#ifndef CORE_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_
#define CORE_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/service_controller.h"
#include "connections/listeners.h"
#include "connections/params.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/listeners.h"
#include "connections/v3/listening_result.h"
#include "connections/v3/params.h"
#include "internal/interop/device.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

// ServiceControllerRouter: this class is an implementation detail of a
// nearby::Core class. The latter delegates all of its activities to
// the former.
//
// All the activities are documented in the public API class:
// cpp/core_v2/core.h
//
// In every method, ::nearby::Borrowable<ClientProxy*> represents the client app
// which receives notifications from Nearby Connections service and forwards
// them to the app. The rest of arguments have the same meaning as the
// corresponding methods in the definition of nearby::Core API.
//
// Every activity is handled the same way:
// 1) all the arguments to the call are captured by value;
// 2) the actual processing is scheduled on a private single-threaded executor,
//    which makes locking unnecessary, when internal data is being manipulated.
// 3) activity handlers are delegating much of their work to an implementation
//    of a ServiceController interface, which does the actual job.
class ServiceControllerRouter {
 public:
  ServiceControllerRouter();
  virtual ~ServiceControllerRouter();
  // Not copyable or movable
  ServiceControllerRouter(const ServiceControllerRouter&) = delete;
  ServiceControllerRouter& operator=(const ServiceControllerRouter&) = delete;
  ServiceControllerRouter(ServiceControllerRouter&&) = delete;
  ServiceControllerRouter& operator=(ServiceControllerRouter&&) = delete;

  virtual v3::Quality GetMediumQuality(Medium medium);
  virtual void StartAdvertising(::nearby::Borrowable<ClientProxy*> client,
                                absl::string_view service_id,
                                const AdvertisingOptions& advertising_options,
                                const ConnectionRequestInfo& info,
                                ResultCallback callback);

  virtual void StopAdvertising(::nearby::Borrowable<ClientProxy*> client,
                               ResultCallback callback);

  virtual void StartDiscovery(::nearby::Borrowable<ClientProxy*> client,
                              absl::string_view service_id,
                              const DiscoveryOptions& discovery_options,
                              const DiscoveryListener& listener,
                              ResultCallback callback);

  virtual void StopDiscovery(::nearby::Borrowable<ClientProxy*> client,
                             ResultCallback callback);

  virtual void InjectEndpoint(::nearby::Borrowable<ClientProxy*> client,
                              absl::string_view service_id,
                              const OutOfBandConnectionMetadata& metadata,
                              ResultCallback callback);

  virtual void RequestConnection(::nearby::Borrowable<ClientProxy*> client,
                                 absl::string_view endpoint_id,
                                 const ConnectionRequestInfo& info,
                                 const ConnectionOptions& connection_options,
                                 ResultCallback callback);

  virtual void AcceptConnection(::nearby::Borrowable<ClientProxy*> client,
                                absl::string_view endpoint_id,
                                PayloadListener listener,
                                ResultCallback callback);

  virtual void RejectConnection(::nearby::Borrowable<ClientProxy*> client,
                                absl::string_view endpoint_id,
                                ResultCallback callback);

  virtual void InitiateBandwidthUpgrade(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view endpoint_id,
      ResultCallback callback);

  virtual void SendPayload(::nearby::Borrowable<ClientProxy*> client,
                           absl::Span<const std::string> endpoint_ids,
                           Payload payload, ResultCallback callback);

  virtual void CancelPayload(::nearby::Borrowable<ClientProxy*> client,
                             std::uint64_t payload_id, ResultCallback callback);

  virtual void DisconnectFromEndpoint(::nearby::Borrowable<ClientProxy*> client,
                                      absl::string_view endpoint_id,
                                      ResultCallback callback);

  ////////////////////////////// V3 ////////////////////////////////////////////
  virtual void StartListeningForIncomingConnectionsV3(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options,
      v3::ListeningResultListener callback);

  virtual void StopListeningForIncomingConnectionsV3(
      ::nearby::Borrowable<ClientProxy*> client);

  virtual void RequestConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                                   const NearbyDevice& remote_device,
                                   v3::ConnectionRequestInfo info,
                                   const ConnectionOptions& connection_options,
                                   ResultCallback callback);

  virtual void AcceptConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                                  const NearbyDevice& remote_device,
                                  v3::PayloadListener listener,
                                  ResultCallback callback);

  virtual void RejectConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                                  const NearbyDevice& remote_device,
                                  ResultCallback callback);

  virtual void InitiateBandwidthUpgradeV3(
      ::nearby::Borrowable<ClientProxy*> client,
      const NearbyDevice& remote_device, ResultCallback callback);

  virtual void SendPayloadV3(::nearby::Borrowable<ClientProxy*> client,
                             const NearbyDevice& recipient_device,
                             Payload payload, ResultCallback callback);

  virtual void CancelPayloadV3(::nearby::Borrowable<ClientProxy*> client,
                               const NearbyDevice& recipient_device,
                               std::uint64_t payload_id,
                               ResultCallback callback);

  virtual void DisconnectFromDeviceV3(::nearby::Borrowable<ClientProxy*> client,
                                      const NearbyDevice& remote_device,
                                      ResultCallback callback);

  virtual void UpdateAdvertisingOptionsV3(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const AdvertisingOptions& advertising_options, ResultCallback callback);

  virtual void UpdateDiscoveryOptionsV3(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      const DiscoveryOptions& discovery_options, ResultCallback callback);
  /////////////////////////////// END V3 ///////////////////////////////////////

  virtual void StopAllEndpoints(::nearby::Borrowable<ClientProxy*> client,
                                ResultCallback callback);

  virtual void SetCustomSavePath(::nearby::Borrowable<ClientProxy*> client,
                                 absl::string_view path,
                                 ResultCallback callback);

  void SetServiceControllerForTesting(
      std::unique_ptr<ServiceController> service_controller);

 private:
  // Lazily create ServiceController.
  ServiceController* GetServiceController();

  void RouteToServiceController(const std::string& name, Runnable runnable);
  void FinishClientSession(::nearby::Borrowable<ClientProxy*> client);

  std::unique_ptr<ServiceController> service_controller_;
  SingleThreadExecutor serializer_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_
