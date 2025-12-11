// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_NETWORK_UTILS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_NETWORK_UTILS_H_

#include "absl/functional/any_invocable.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/listeners.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/service_address.h"

@class GNCNWFramework;
@class GNCNWFrameworkServerSocket;
@class GNCNWFrameworkSocket;

namespace nearby {
namespace apple {
namespace network_utils {

/**
 * Callback for discovered services.
 */
struct NetworkDiscoveredServiceCallback {
  absl::AnyInvocable<void(const NsdServiceInfo& service_info)> network_service_discovered_cb =
      DefaultCallback<const NsdServiceInfo&>();
  absl::AnyInvocable<void(const NsdServiceInfo& service_info)> network_service_lost_cb =
      DefaultCallback<const NsdServiceInfo&>();
};

/**
 * Starts listening for incoming connections on the specified port.
 * @param medium The NW framework to use.
 * @param port The port to listen on.
 * @param include_peer_to_peer Whether to include peer-to-peer connections.
 * @return A pointer to the server socket.
 */
GNCNWFrameworkServerSocket* ListenForService(GNCNWFramework* medium, int port,
                                             bool include_peer_to_peer);

/**
 * Starts listening for incoming connections using PSK based TLS on the specified port.
 * @param medium The NW framework to use.
 * @param psk_info The PSK info to use for listening.
 * @param port The port to listen on.
 * @param include_peer_to_peer Whether to include peer-to-peer connections.
 * @return A pointer to the server socket.
 */
GNCNWFrameworkServerSocket* ListenForService(GNCNWFramework* medium, const api::PskInfo& psk_info,
                                             int port, bool include_peer_to_peer);

/**
 *  Starts advertising a service with the specified service info.
 *
 *  @param medium The NW framework to use.
 *  @param nsd_service_info The service info to advertise.
 *  @return True if the service was successfully started.
 */
bool StartAdvertising(GNCNWFramework* medium, const NsdServiceInfo& nsd_service_info);

/**
 *  Stops advertising a service with the specified service info.
 *
 *  @param medium The NW framework to use.
 *  @param nsd_service_info The service info to stop advertising.
 *  @return True if the service was successfully stopped.
 */
bool StopAdvertising(GNCNWFramework* medium, const NsdServiceInfo& nsd_service_info);

/**
 *  Connects to the specified service.
 *
 *  @param medium The NW framework to use.
 *  @param remote_service_info The service info to connect to.
 *  @param cancellation_flag The cancellation flag to use.
 *  @return A pointer to the socket.
 */
GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const NsdServiceInfo& remote_service_info,
                                       CancellationFlag* cancellation_flag);

/**
 *  Starts discovery for services of the specified type.
 *
 *  @param medium The NW framework to use.
 *  @param service_type The service type to discover.
 *  @param callback The callback to call when a service is discovered or lost.
 *  @param include_peer_to_peer Whether to include peer-to-peer connections.
 *  @return True if the service was successfully started.
 */
bool StartDiscovery(GNCNWFramework* medium, const std::string& service_type,
                    NetworkDiscoveredServiceCallback callback, bool include_peer_to_peer);

/**
 *  Stops discovery for services of the specified type.
 *
 *  @param medium The NW framework to use.
 *  @param service_type The service type to stop discovery for.
 *  @return True if the service was successfully stopped.
 */
bool StopDiscovery(GNCNWFramework* medium, const std::string& service_type);

/**
 * Connects to the specified service.
 *
 * @param medium The NW framework to use.
 * @param remote_service_info The service info to connect to.
 * @param cancellation_flag The cancellation flag to use.
 * @return A pointer to the socket.
 */
GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const NsdServiceInfo& remote_service_info,
                                       CancellationFlag* cancellation_flag);

/**
 * Connects to the specified service using PSK based TLS.
 *
 * @param medium The NW framework to use.
 * @param remote_service_info The service info to connect to.
 * @param psk_info The PSK info to use for connection.
 * @param cancellation_flag The cancellation flag to use.
 * @return A pointer to the socket.
 */
GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const NsdServiceInfo& remote_service_info,
                                       const api::PskInfo& psk_info,
                                       CancellationFlag* cancellation_flag);

/**
 * Connects to the specified service with ip address and port.
 *
 * @param medium The NW framework to use.
 * @param ip_address The ip address of the service to connect to.
 * @param port The port of the service to connect to.
 * @param include_peer_to_peer Whether to include peer-to-peer connections.
 * @param cancellation_flag The cancellation flag to use.
 * @return A pointer to the socket.
 */
GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const ServiceAddress& service_address,
                                       bool include_peer_to_peer,
                                       CancellationFlag* cancellation_flag);
}  // namespace network_utils
}  // namespace apple
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_NETWORK_UTILS_H_
