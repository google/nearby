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

#include <set>
#include <vector>

#include "core/internal/client_proxy.h"
#include "core/internal/service_controller.h"
#include "core/params.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {
namespace connections {

namespace service_controller_router {

template <typename>
class StartAdvertisingRunnable;
template <typename>
class StopAdvertisingRunnable;
template <typename>
class StartDiscoveryRunnable;
template <typename>
class StopDiscoveryRunnable;
template <typename>
class SendConnectionRequestRunnable;
template <typename>
class AcceptConnectionRequestRunnable;
template <typename>
class RejectConnectionRequestRunnable;
template <typename>
class InitiateBandwidthUpgradeRunnable;
template <typename>
class SendPayloadRunnable;
template <typename>
class CancelPayloadRunnable;
template <typename>
class DisconnectFromEndpointRunnable;
template <typename>
class StopAllEndpointsRunnable;
template <typename>
class ClientDisconnectingRunnable;

}  // namespace service_controller_router

template <typename Platform>
class ServiceControllerRouter {
 public:
  ServiceControllerRouter();
  ~ServiceControllerRouter();

  void startAdvertising(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StartAdvertisingParams> start_advertising_params);
  void stopAdvertising(Ptr<ClientProxy<Platform> > client_proxy,
                       ConstPtr<StopAdvertisingParams> stop_advertising_params);

  void startDiscovery(Ptr<ClientProxy<Platform> > client_proxy,
                      ConstPtr<StartDiscoveryParams> start_discovery_params);
  void stopDiscovery(Ptr<ClientProxy<Platform> > client_proxy,
                     ConstPtr<StopDiscoveryParams> stop_discovery_params);

  void requestConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<RequestConnectionParams> request_connection_params);
  void acceptConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<AcceptConnectionParams> accept_connection_params);
  void rejectConnection(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<RejectConnectionParams> reject_connection_params);

  void initiateBandwidthUpgrade(Ptr<ClientProxy<Platform> > client_proxy,
                                ConstPtr<InitiateBandwidthUpgradeParams>
                                    initiate_bandwidth_upgrade_params);

  void sendPayload(Ptr<ClientProxy<Platform> > client_proxy,
                   ConstPtr<SendPayloadParams> send_payload_params);
  void cancelPayload(Ptr<ClientProxy<Platform> > client_proxy,
                     ConstPtr<CancelPayloadParams> cancel_payload_params);

  void disconnectFromEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<DisconnectFromEndpointParams> disconnect_from_endpoint_params);
  void stopAllEndpoints(
      Ptr<ClientProxy<Platform> > client_proxy,
      ConstPtr<StopAllEndpointsParams> stop_all_endpoint_params);

  void clientDisconnecting(Ptr<ClientProxy<Platform> > client_proxy);

 private:
  template <typename>
  friend class service_controller_router::StartAdvertisingRunnable;
  template <typename>
  friend class service_controller_router::StopAdvertisingRunnable;
  template <typename>
  friend class service_controller_router::StartDiscoveryRunnable;
  template <typename>
  friend class service_controller_router::StopDiscoveryRunnable;
  template <typename>
  friend class service_controller_router::SendConnectionRequestRunnable;
  template <typename>
  friend class service_controller_router::AcceptConnectionRequestRunnable;
  template <typename>
  friend class service_controller_router::RejectConnectionRequestRunnable;
  template <typename>
  friend class service_controller_router::InitiateBandwidthUpgradeRunnable;
  template <typename>
  friend class service_controller_router::SendPayloadRunnable;
  template <typename>
  friend class service_controller_router::CancelPayloadRunnable;
  template <typename>
  friend class service_controller_router::DisconnectFromEndpointRunnable;
  template <typename>
  friend class service_controller_router::StopAllEndpointsRunnable;
  template <typename>
  friend class service_controller_router::ClientDisconnectingRunnable;

  static bool clientHasConnectionToAtLeastOneEndpoint(
      Ptr<ClientProxy<Platform> > client_proxy,
      const std::vector<std::string>& remote_endpoint_ids);

  void routeToServiceController(Ptr<Runnable> runnable);

  Status::Value acquireServiceControllerForClient(
      Ptr<ClientProxy<Platform> > client_proxy, const Strategy& strategy);
  bool clientHasAquiredServiceController(
      Ptr<ClientProxy<Platform> > client_proxy);
  void releaseServiceControllerForClient(
      Ptr<ClientProxy<Platform> > client_proxy);
  void doneWithStrategySessionForClient(
      Ptr<ClientProxy<Platform> > client_proxy);
  Status::Value updateCurrentServiceControllerAndStrategy(
      const Strategy& strategy);

  std::set<Ptr<ClientProxy<Platform> > > current_service_controller_clients_;
  Ptr<ServiceController<Platform> > current_service_controller_;
  Ptr<Strategy> current_strategy_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > serializer_;
  std::shared_ptr<ServiceControllerRouter<Platform>> self_{this, [](void*){}};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/service_controller_router.cc"

#endif  // CORE_INTERNAL_SERVICE_CONTROLLER_ROUTER_H_
