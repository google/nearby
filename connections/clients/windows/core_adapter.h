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
#ifndef LOCATION_NEARBY_CONNECTIONS_WINDOWS_CORE_ADAPTER_H_
#define LOCATION_NEARBY_CONNECTIONS_WINDOWS_CORE_ADAPTER_H_

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

// todo(jfcarroll) This cannot remain. It exposes stuff the client doesn't need.
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/offline_service_controller.h"

#define DLL_EXPORT extern "C" __declspec(dllexport)

namespace location {
namespace nearby {
namespace connections {
class Core;
class ServiceControllerRouter;

namespace windows {

// Initizlizes a Core instance, providing the ServiceController factory from
// app side. If no factory is provided, it will initialize a new
// factory creating OffilineServiceController.
// Returns the instance handle to c# client.
DLL_EXPORT connections::Core *__stdcall InitCoreWithServiceControllerFactory(
    std::function<connections::ServiceController *()> factory = []() {
      return new connections::OfflineServiceController;
    });

// Initializes a default Core instance.
// Returns the instance handle to c# client.
DLL_EXPORT Core *__stdcall InitCore(ServiceControllerRouter *router);

// Closes the core with stopping all endpoints, then free the memory.
DLL_EXPORT void __stdcall CloseCore(Core *pCore);

// Starts advertising an endpoint for a local app.
//
// service_id - An identifier to advertise your app to other endpoints.
//              This can be an arbitrary string, so long as it uniquely
//              identifies your service. A good default is to use your
//              app's package name.
// advertising_options - The options for advertising.
// info       - Connection parameters:
// > name     - A human readable name for this endpoint, to appear on
//              other devices.
// > listener - A callback notified when remote endpoints request a
//              connection to this endpoint.
// callback - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if advertising started successfully.
//     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DLL_EXPORT void __stdcall StartAdvertising(
    Core *pCore, const char *service_id, AdvertisingOptions advertising_options,
    connections::ConnectionRequestInfo info, ResultCallback callback);

// Stops advertising a local endpoint. Should be called after calling
// StartAdvertising, as soon as the application no longer needs to advertise
// itself or goes inactive. Payloads can still be sent to connected
// endpoints after advertising ends.
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DLL_EXPORT void __stdcall StopAdvertising(Core *pCore, ResultCallback callback);

// Starts discovery for remote endpoints with the specified service ID.
//
// service_id - The ID for the service to be discovered, as specified in
//              the corresponding call to StartAdvertising.
// listener   - A callback notified when a remote endpoint is discovered.
// discovery_options - The options for discovery.
// result_cb  - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if discovery started successfully.
//     Status::STATUS_ALREADY_DISCOVERING if the app is already
//         discovering the specified service.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DLL_EXPORT void __stdcall StartDiscovery(Core *pCore, const char *service_id,
                                         DiscoveryOptions discovery_options,
                                         DiscoveryListener listener,
                                         ResultCallback callback);

// Stops discovery for remote endpoints, after a previous call to
// StartDiscovery, when the client no longer needs to discover endpoints or
// goes inactive. Payloads can still be sent to connected endpoints after
// discovery ends.
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DLL_EXPORT void __stdcall StopDiscovery(connections::Core *pCore,
                                        connections::ResultCallback callback);

// Invokes the discovery callback from a previous call to StartDiscovery()
// with the given endpoint info. The previous call to StartDiscovery() must
// have been passed ConnectionOptions with is_out_of_band_connection == true.
//
// service_id            - The ID for the service to be discovered, as
//                         specified in the corresponding call to
//                         StartDiscovery().
// metadata              - Metadata used in order to inject the endpoint.
// result_cb             - to access the status of the operation when
//                         available.
//   Possible status codes include:
//     Status::kSuccess if endpoint injection was attempted.
//     Status::kError if endpoint_id, endpoint_info, or
//         remote_bluetooth_mac_address are malformed.
//     Status::kOutOfOrderApiCall if the app is not discovering.
DLL_EXPORT void __stdcall InjectEndpoint(Core *pCore, char *service_id,
                                         OutOfBandConnectionMetadata metadata,
                                         ResultCallback callback);

// Sends a request to connect to a remote endpoint.
//
// endpoint_id - The identifier for the remote endpoint to which a
//               connection request will be sent. Should match the value
//               provided in a call to
//               DiscoveryListener::endpoint_found_cb()
// info        - Connection parameters:
// > name      - A human readable name for the local endpoint, to appear on
//               the remote endpoint.
// > listener  - A callback notified when the remote endpoint sends a
//               response to the connection request.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if the connection request was sent.
//     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already
//         has a connection to the specified endpoint.
//     Status::STATUS_RADIO_ERROR if we failed to connect because of an
//         issue with Bluetooth/WiFi.
//     Status::STATUS_ERROR if we failed to connect for any other reason.
DLL_EXPORT void __stdcall RequestConnection(
    Core *pCore, const char *endpoint_id, ConnectionRequestInfo info,
    ConnectionOptions connection_options, ResultCallback callback);

// Accepts a connection to a remote endpoint. This method must be called
// before Payloads can be exchanged with the remote endpoint.
//
// endpoint_id - The identifier for the remote endpoint. Should match the
//               value provided in a call to
//               ConnectionListener::onConnectionInitiated.
// listener    - A callback for payloads exchanged with the remote endpoint.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if the connection request was accepted.
//     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already.
//         has a connection to the specified endpoint.
DLL_EXPORT void __stdcall AcceptConnection(Core *pCore, const char *endpoint_id,
                                           PayloadListener listener,
                                           ResultCallback callback);

// Rejects a connection to a remote endpoint.
//
// endpoint_id - The identifier for the remote endpoint. Should match the
//               value provided in a call to
//               ConnectionListener::onConnectionInitiated().
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK} if the connection request was rejected.
//     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT} if the app already
//         has a connection to the specified endpoint.
DLL_EXPORT void __stdcall RejectConnection(Core *pCore, const char *endpoint_id,
                                           ResultCallback callback);

// Sends a Payload to a remote endpoint. Payloads can only be sent to remote
// endpoints once a notice of connection acceptance has been delivered via
// ConnectionListener::onConnectionResult().
//
// endpoint_ids - Array of remote endpoint identifiers for the  to which the
//                payload should be sent.
// payload      - The Payload to be sent.
// result_cb    - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the device has not first
//         performed advertisement or discovery (to set the Strategy.)
//     Status::STATUS_ENDPOINT_UNKNOWN if there's no active (or pending)
//         connection to the remote endpoint.
//     Status::STATUS_OK if none of the above errors occurred. Note that this
//         indicates that Nearby Connections will attempt to send the Payload,
//         but not that the send has successfully completed yet. Errors might
//         still occur during transmission (and at different times for
//         different endpoints), and will be delivered via
//         PayloadCallback#onPayloadTransferUpdate.
DLL_EXPORT void __stdcall SendPayload(
    Core *pCore, absl::Span<const std::string> endpoint_ids, Payload payload,
    ResultCallback callback);

// Cancels a Payload currently in-flight to or from remote endpoint(s).
//
// payload_id - The identifier for the Payload to be canceled.
// result_cb  - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DLL_EXPORT void __stdcall CancelPayload(Core *pCore, int64_t payload_id,
                                        ResultCallback callback);

// Disconnects from a remote endpoint. {@link Payload}s can no longer be sent
// to or received from the endpoint after this method is called.
//
// endpoint_id - The identifier for the remote endpoint to disconnect from.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK - finished successfully.
DLL_EXPORT void __stdcall DisconnectFromEndpoint(Core *pCore, char *endpoint_id,
                                                 ResultCallback callback);

// Disconnects from, and removes all traces of, all connected and/or
// discovered endpoints. This call is expected to be preceded by a call to
// StopAdvertising or StartDiscovery as needed. After calling
// StopAllEndpoints, no further operations with remote endpoints will be
// possible until a new call to one of StartAdvertising() or StartDiscovery().
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK - finished successfully.
DLL_EXPORT void __stdcall StopAllEndpoints(Core *pCore,
                                           ResultCallback callback);

// Sends a request to initiate connection bandwidth upgrade.
//
// endpoint_id - The identifier for the remote endpoint which will be
//               switching to a higher connection data rate and possibly
//               different wireless protocol. On success, calls
//               ConnectionListener::bandwidth_changed_cb().
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK - finished successfully.
DLL_EXPORT void __stdcall InitiateBandwidthUpgrade(Core *pCore,
                                                   char *endpoint_id,
                                                   ResultCallback callback);

// Gets the local endpoint generated by Nearby Connections.
DLL_EXPORT const char *__stdcall GetLocalEndpointId(Core *pCore);

// Initializes a default ServiceControllerRouter instance.
// Returns the instance handle to c# client.
DLL_EXPORT ServiceControllerRouter *__stdcall InitServiceControllerRouter();

// Close a ServiceControllerRouter instance.
DLL_EXPORT void __stdcall CloseServiceControllerRouter(
    ServiceControllerRouter *pRouter);

}  // namespace windows
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // LOCATION_NEARBY_CONNECTIONS_WINDOWS_CORE_ADAPTER_H_
