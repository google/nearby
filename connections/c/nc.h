// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_H_

#include <stddef.h>

#include "connections/c/nc_def.h"
#include "connections/c/nc_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Creates a new Nearby Connections service.
NC_API NC_INSTANCE NcCreateService();
NC_API void NcCloseService(NC_INSTANCE instance);

// Starts adververtising an endpoint using Nearby Connections.
//
// instance - The returned instance by NcOpenService.
// service_id - The ID for the service to be advertised.
// advertising_options - The options for advertising.
// connection_request_info - Connection parameters, including endpoint info
//   and listeners.
// result_callback - The result of the API operation.
NC_API void NcStartAdvertising(
    NC_INSTANCE instance, const NC_DATA* service_id,
    const NC_ADVERTISING_OPTIONS* advertising_options,
    const NC_CONNECTION_REQUEST_INFO* connection_request_info,
    NcCallbackResult result_callback, CALLER_CONTEXT context);

// Stops advertising a local endpoint. It should be called after calling
// StartAdvertising.
//
// instance - The instance is used to start advertising.
// result_callback - The result of the API operation.
NC_API void NcStopAdvertising(NC_INSTANCE instance,
                              NcCallbackResult result_callback,
                              CALLER_CONTEXT context);

// Starts to discover for remote endpoints with the specified service ID.
//
// instance - The returned instance by NcOpenService.
// service_id - The ID for the service to be discovered.
// discovery_options - The options for discovery.
// discovery_listener - The callbacks notified when a remote endpoint is
//   reported.
// result_callback - The result of the API operation.
NC_API void NcStartDiscovery(NC_INSTANCE instance, const NC_DATA* service_id,
                             const NC_DISCOVERY_OPTIONS* discovery_options,
                             const NC_DISCOVERY_LISTENER* discovery_listener,
                             NcCallbackResult result_callback,
                             CALLER_CONTEXT context);

// Stops discovering for a running discovery.
//
// instance - The instance is used to start discovery.
// result_callback - The result of the API operation.
NC_API void NcStopDiscovery(NC_INSTANCE instance,
                            NcCallbackResult result_callback,
                            CALLER_CONTEXT context);

// Invokes the discovery callback from a previous call to NcStartDiscovery()
// with the given endpoint info. The previous call to NcStartDiscovery() must
// have been passed ConnectionOptions with is_out_of_band_connection == true.
//
// instance - The instance is used to start discovery.
// service_id - The ID for the service to be discovered, as specified in the
//   corresponding call to NcStartDiscovery().
// metadata - Metadata used in order to inject the endpoint.
// result_callback - The result of the API operation.
NC_API void NcInjectEndpoint(NC_INSTANCE instance, const NC_DATA* service_id,
                             const NC_OUT_OF_BAND_CONNECTION_METADATA* metadata,
                             NcCallbackResult result_callback,
                             CALLER_CONTEXT context);

// Sends a request to connect to a remote endpoint.
//
// instance - The returned instance by NcOpenService.
// endpoint_id - The identifier for the remote endpoint to which a
//   connection request will be sent.
// connection_request_info - Connection parameters.
// connection_options - Options to connect.
// result_callback - The result of the API operation.
NC_API void NcRequestConnection(
    NC_INSTANCE instance, int endpoint_id,
    const NC_CONNECTION_REQUEST_INFO* connection_request_info,
    const NC_CONNECTION_OPTIONS* connection_options,
    NcCallbackResult result_callback, CALLER_CONTEXT context);

// Accepts a connection to a remote endpoint.
//
// instance - The returned instance by NcOpenService.
// endpoint_id - The identifier for the remote endpoint.
// payload_listener - A callback for payloads exchanged with the remote
//   endpoint.
// result_callback - The result of the API operation.
NC_API void NcAcceptConnection(NC_INSTANCE instance, int endpoint_id,
                               NC_PAYLOAD_LISTENER payload_listener,
                               NcCallbackResult result_callback,
                               CALLER_CONTEXT context);

// Rejects a connection from a remote endpoint.
//
// instance - The returned instance by NcOpenService.
// endpoint_id - The identifier for the remote endpoint.
// result_callback - The result of the API operation.
NC_API void NcRejectConnection(NC_INSTANCE instance, int endpoint_id,
                               NcCallbackResult result_callback,
                               CALLER_CONTEXT context);

// Sends a Payload to a remote endpoint.
//
// instance - The returned instance by NcOpenService.
// endpoint_ids_size - The endpoint number to receive the payload.
// endpoint_ids - The endpoint ID array.
// payload - the payload will be sent.
// result_callback - The result of the API operation.
NC_API void NcSendPayload(NC_INSTANCE instance, size_t endpoint_ids_size,
                          const int* endpoint_ids, const NC_PAYLOAD* payload,
                          NcCallbackResult result_callback,
                          CALLER_CONTEXT context);

// Cancels a Payload currently in-flight to or from remote endpoint(s).
//
// instance - The Nearby Connections instance is called by NcSendPayload.
// payload_id - The payload ID of payload to cancel.
// result_callback - The result of the API operation.
NC_API void NcCancelPayload(NC_INSTANCE instance, NC_PAYLOAD_ID payload_id,
                            NcCallbackResult result_callback,
                            CALLER_CONTEXT context);

// Disconnects from a remote endpoint.
//
// instance - The returned instance by NcOpenService.
// endpoint_id - The endpoint ID of remote device to disconnect.
// result_callback - The result of the API operation.
NC_API void NcDisconnectFromEndpoint(NC_INSTANCE instance, int endpoint_id,
                                     NcCallbackResult result_callback,
                                     CALLER_CONTEXT context);

// Disconnects from, and removes all traces of, all connected and/or
// discovered endpoints.
//
// instance - The returned instance by NcOpenService.
// result_callback - The result of the API operation.
NC_API void NcStopAllEndpoints(NC_INSTANCE instance,
                               NcCallbackResult result_callback,
                               CALLER_CONTEXT context);

// Sends a request to initiate connection bandwidth upgrade.
//
// instance - The returned instance by NcOpenService.
// endpoint_id - Requested to upgrade on the remote device with the endpoint ID.
// result_callback - The result of the API operation.
NC_API void NcInitiateBandwidthUpgrade(NC_INSTANCE instance, int endpoint_id,
                                       NcCallbackResult result_callback,
                                       CALLER_CONTEXT context);

// Gets the local endpoint generated by Nearby Connections.
NC_API int NcGetLocalEndpointId(NC_INSTANCE instance);

// Enable/Disable BLE V2 advertising. The method should be deprecated after
// BLE V1 deprecated.
NC_API void NcEnableBleV2(NC_INSTANCE instance, bool enable,
                          NcCallbackResult result_callback,
                          CALLER_CONTEXT context);

// Sets the custom save path for Nearby Connections.
NC_API void NcSetCustomSavePath(NC_INSTANCE instance, const NC_DATA* save_path,
                                NcCallbackResult result_callback,
                                CALLER_CONTEXT context);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_H_
