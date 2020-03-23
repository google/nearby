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
 * The Platform class must also provide typedefs for the following subset of
 * primitives to identify the concrete classes:
 *
 *   SingleThreadExecutorType
 *   MultiThreadExecutorType
 *   ScheduledExecutorType
 *
 * A sample Platform class can be found at
 * //platform/impl/sample/sample_platform.h
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
