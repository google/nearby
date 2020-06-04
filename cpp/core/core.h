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

#ifndef CORE_CORE_H_
#define CORE_CORE_H_

#include "core/internal/client_proxy.h"
#include "core/internal/service_controller_router.h"
#include "core/params.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

/*
 * This class defines the API of the Nearby Connections Core library.
 *
 * Each passed-in Platform must provide a set of primitives with platform-
 * specific implementations. The Platform class must provide factory functions
 * for the following primitives:
 *
 *   SingleThreadExecutor
 *   MultiThreadExecutor
 *   ScheduledExecutor
 *   Lock
 *   CountDownLatch
 *   AtomicBoolean
 *   AtomicReference
 *   SettableFuture
 *   BluetoothAdapter
 *   BluetoothClassicMedium
 *   HashUtils
 *   ThreadUtils
 *   SystemClock
 *   ConditionVariable
 *
 * A sample Platform definitions can be found at
 * //platform/impl/shared/sample/sample_platform.cc
 *
 * It is no longer necessary to parametrize system types with a platform type.
 * New, recommended approach is to define platform support by implementing
 * static methods of "location::nearby::platform::ImplementationPlatform" class.
 * every library class that needs platform support, must include platform
 * header "platform/api/platform.h" and use it.
 * To keep textual compatibility, one could define the following alias
 * "using Platform = platform::ImplementationPlatform;".
 * this will replace the "template <typename Platform>" declaration.
 *
 * As an added benefit, this will allow to not include *.cc files from *.h,
 * and let more static analysis happen at compiler stage.
 */
template <typename Platform>
class Core {
 public:
  Core();
  ~Core();

  void startAdvertising(
      ConstPtr<StartAdvertisingParams> start_advertising_params);
  void stopAdvertising(ConstPtr<StopAdvertisingParams> stop_advertising_params);
  void startDiscovery(ConstPtr<StartDiscoveryParams> start_discovery_params);
  void stopDiscovery(ConstPtr<StopDiscoveryParams> stop_discovery_params);
  void requestConnection(
      ConstPtr<RequestConnectionParams> request_connection_params);
  void acceptConnection(
      ConstPtr<AcceptConnectionParams> accept_connection_params);
  void rejectConnection(
      ConstPtr<RejectConnectionParams> reject_connection_params);
  void initiateBandwidthUpgrade(ConstPtr<InitiateBandwidthUpgradeParams>
                                    initiate_bandwidth_upgrade_params);
  void sendPayload(ConstPtr<SendPayloadParams> send_payload_params);
  void cancelPayload(ConstPtr<CancelPayloadParams> cancel_payload_params);
  void disconnectFromEndpoint(
      ConstPtr<DisconnectFromEndpointParams> disconnect_from_endpoint_params);
  void stopAllEndpoints(
      ConstPtr<StopAllEndpointsParams> stop_all_endpoints_params);

 private:
  ScopedPtr<Ptr<ClientProxy<Platform> > > client_proxy_;
  ScopedPtr<Ptr<ServiceControllerRouter<Platform> > >
      service_controller_router_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/core.cc"

#endif  // CORE_CORE_H_
