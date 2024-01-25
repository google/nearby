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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DART_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DART_H_

#include "third_party/dart_lang/v2/runtime/include/dart_api_dl.h"
#include "third_party/dart_lang/v2/runtime/include/dart_native_api.h"
#include "connections/c/nc.h"
#include "connections/c/nc_types.h"
#include "connections/dart/nc_adapter_def.h"

#ifdef __cplusplus
extern "C" {
#endif

enum StrategyDart {
  // LINT.IfChange
  STRATEGY_UNKNOWN = -1,
  STRATEGY_P2P_CLUSTER = 0,
  STRATEGY_P2P_STAR,
  STRATEGY_P2P_POINT_TO_POINT,
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/strategy.dart)
};

enum PayloadTypeDart {
  // LINT.IfChange
  PAYLOAD_TYPE_UNKNOWN = 0,
  PAYLOAD_TYPE_BYTE,
  PAYLOAD_TYPE_STREAM,
  PAYLOAD_TYPE_FILE,
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload.dart)
};

struct MediumsDart {
  // LINT.IfChange
  int64_t bluetooth;
  int64_t ble;
  int64_t wifi_lan;
  int64_t wifi_hotspot;
  int64_t web_rtc;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/mediums.dart)
};

struct AdvertisingOptionsDart {
  // LINT.IfChange
  StrategyDart strategy;
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;
  int64_t low_power;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t is_out_of_band_connection = false;
  const char *fast_advertisement_service_uuid;

  // The information about this device (eg. name, device type),
  // to appear on the remote device.
  // Defined by client/application.
  const char *device_info;

  MediumsDart mediums;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/advertising_options.dart)
};

struct ConnectionOptionsDart {
  // LINT.IfChange
  StrategyDart strategy;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;
  int64_t low_power;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t is_out_of_band_connection = false;
  char *remote_bluetooth_mac_address;
  char *fast_advertisement_service_uuid;
  int64_t keep_alive_interval_millis;
  int64_t keep_alive_timeout_millis;

  MediumsDart mediums;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_options.dart)
};

struct DiscoveryOptionsDart {
  // LINT.IfChange
  StrategyDart strategy;
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t is_out_of_band_connection = false;
  const char *fast_advertisement_service_uuid;
  const char *remote_bluetooth_mac_address;

  MediumsDart mediums;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/discovery_options.dart)
};

struct DiscoveryListenerDart {
  // LINT.IfChange
  int64_t found_dart_port;
  int64_t lost_dart_port;
  int64_t distance_changed_dart_port;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/discovery_listener.dart)
};

struct PayloadListenerDart {
  // LINT.IfChange
  int64_t initial_byte_info_port;
  int64_t initial_stream_info_port;
  int64_t initial_file_info_port;
  int64_t payload_progress_dart_port;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload_listener.dart)
};

struct ConnectionListenerDart {
  // LINT.IfChange
  int64_t initiated_dart_port;
  int64_t accepted_dart_port;
  int64_t rejected_dart_port;
  int64_t disconnected_dart_port;
  int64_t bandwidth_changed_dart_port;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_listener.dart)
};

struct ConnectionRequestInfoDart {
  // LINT.IfChange
  int endpoint_info_size;
  char *endpoint_info;
  ConnectionListenerDart connection_listener;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_request_info.dart)
};

struct PayloadDart {
  // LINT.IfChange
  int64_t id;
  PayloadTypeDart type;
  int64_t size;
  char *data;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload.dart)
};

static void ResultCB(NC_STATUS status);

static void ListenerInitiatedCB(
    const char *endpoint_id,
    const NC_CONNECTION_RESPONSE_INFO &connection_response_info);
static void ListenerAcceptedCB(const char *endpoint_id);
static void ListenerRejectedCB(const char *endpoint_id, NC_STATUS status);
static void ListenerDisconnectedCB(const char *endpoint_id);
static void ListenerBandwidthChangedCB(const char *endpoint_id,
                                       NC_MEDIUM medium);
static void ListenerEndpointFoundCB(const char *endpoint_id,
                                    const NC_DATA &endpoint_info,
                                    const char *service_id);
static void ListenerEndpointLostCB(const char *endpoint_id);
static void ListenerEndpointDistanceChangedCB(const char *endpoint_id,
                                              NC_DISTANCE_INFO distance_info);
static void ListenerPayloadCB(const char *endpoint_id,
                              const NC_PAYLOAD &payload);
static void ListenerPayloadProgressCB(
    const char *endpoint_id,
    const NC_PAYLOAD_PROGRESS_INFO &payload_progress_info);

DART_API NC_INSTANCE OpenServiceDart();
DART_API void CloseServiceDart(NC_INSTANCE instance);

DART_API char* GetLocalEndpointIdDart(NC_INSTANCE instance);

DART_API void EnableBleV2Dart(NC_INSTANCE instance, int64_t enable,
                                        Dart_Port result_cb);

// Starts advertising an endpoint for a local app.
//
// service_id - An identifier to advertise your app to other endpoints.
//              This can be an arbitrary string, so long as it uniquely
//              identifies your service. A good default is to use your
//              app's package name.
// options_dart - options for advertising
// info_dart - Including callbacks notified when remote
//              endpoints request a connection to this endpoint.
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if advertising started successfully.
//     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DART_API void StartAdvertisingDart(
    NC_INSTANCE instance, const char *service_id,
    AdvertisingOptionsDart options_dart, ConnectionRequestInfoDart info_dart,
    Dart_Port result_cb);

// Stops advertising a local endpoint. Should be called after calling
// StartAdvertising, as soon as the application no longer needs to advertise
// itself or goes inactive. Payloads can still be sent to connected
// endpoints after advertising ends.
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DART_API void StopAdvertisingDart(NC_INSTANCE instance,
                                            Dart_Port result_cb);

// Starts discovery for remote endpoints with the specified service ID.
//
// service_id - The ID for the service to be discovered, as specified in
//              the corresponding call to StartAdvertising.
// options    - The options for discovery.
// listener   - Callbacks notified when a remote endpoint is discovered.
// result_cb  - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if discovery started successfully.
//     Status::STATUS_ALREADY_DISCOVERING if the app is already
//         discovering the specified service.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DART_API void StartDiscoveryDart(NC_INSTANCE instance,
                                           const char *service_id,
                                           DiscoveryOptionsDart options_dart,
                                           DiscoveryListenerDart listener_dart,
                                           Dart_Port result_cb);

// Stops discovery for remote endpoints, after a previous call to
// StartDiscovery, when the client no longer needs to discover endpoints or
// goes inactive. Payloads can still be sent to connected endpoints after
// discovery ends.
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DART_API void StopDiscoveryDart(NC_INSTANCE instance,
                                          Dart_Port result_cb);

// Sends a request to connect to a remote endpoint.
//
// endpoint_id - The identifier for the remote endpoint to which a
//               connection request will be sent. Should match the value
//               provided in a call to
//               DiscoveryListener::endpoint_found_cb()
// options_dart - The options for connection.
// info_dart   - Connection parameters:
// > name      - A human readable name for the local endpoint, to appear on
//               the remote endpoint.
// > listener  - Callbacks notified when the remote endpoint sends a
//               response to the connection request.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if the connection request was sent.
//     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already
//         has a connection to the specified endpoint.
//     Status::STATUS_RADIO_ERROR if we failed to connect because of an
//         issue with Bluetooth/WiFi.
//     Status::STATUS_ERROR if we failed to connect for any other reason.
DART_API void RequestConnectionDart(
    NC_INSTANCE instance, const char *endpoint_id,
    ConnectionOptionsDart options_dart, ConnectionRequestInfoDart info_dart,
    Dart_Port result_cb);

// Accepts a connection to a remote endpoint. This method must be called
// before Payloads can be exchanged with the remote endpoint.
//
// endpoint_id - The identifier for the remote endpoint. Should match the
//               value provided in a call to
//               ConnectionListener::onConnectionInitiated.
// listener_dart - A callback for payloads exchanged with the remote endpoint.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if the connection request was accepted.
//     Status::STATUS_ALREADY_CONNECTED_TO_ENDPOINT if the app already.
//         has a connection to the specified endpoint.
DART_API void AcceptConnectionDart(NC_INSTANCE instance,
                                             const char *endpoint_id,
                                             PayloadListenerDart listener_dart,
                                             Dart_Port result_cb);

DART_API void RejectConnectionDart(NC_INSTANCE instance,
                                             const char *endpoint_id,
                                             Dart_Port result_cb);

// Disconnects from a remote endpoint. {@link Payload}s can no longer be sent
// to or received from the endpoint after this method is called.
// endpoint_id - The identifier for the remote endpoint to disconnect from.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK - finished successfully.
DART_API void DisconnectFromEndpointDart(NC_INSTANCE instance,
                                                   char *endpoint_id,
                                                   Dart_Port result_cb);

// Sends a Payload to a remote endpoint. Payloads can only be sent to remote
// endpoints once a notice of connection acceptance has been delivered via
// ConnectionListener::onConnectionResult().
//
// endpoint_id - Remote endpoint identifier for the  to which the
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
DART_API void SendPayloadDart(NC_INSTANCE instance,
                                        const char *endpoint_id,
                                        PayloadDart payload_dart,
                                        Dart_Port result_cb);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_DART_H_
