// Copyright 2021-2022 Google LLC
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

#ifndef LOCATION_NEARBY_CONNECTIONS_DART_CORE_ADAPTER_DART_H_
#define LOCATION_NEARBY_CONNECTIONS_DART_CORE_ADAPTER_DART_H_

#include "third_party/dart_lang/v2/runtime/include/dart_api_dl.h"
#include "third_party/dart_lang/v2/runtime/include/dart_native_api.h"
#include "connections/c/core_adapter.h"

namespace nearby::windows {

enum class StrategyDart {
  P2P_CLUSTER = 0,
  P2P_STAR,
  P2P_POINT_TO_POINT,
};

enum PayloadType {
  // LINT.IfChange
  UNKNOWN = 0,
  BYTE,
  STREAM,
  FILE,
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload.dart)
};

struct Mediums {
  // LINT.IfChange
  int64_t bluetooth;
  int64_t ble;
  int64_t wifi_lan;
  int64_t wifi_hotspot;
  int64_t web_rtc;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/mediums.dart)
};

extern "C" {

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

  Mediums mediums;
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

  Mediums mediums;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_options.dart)
};

struct DiscoveryOptionsDart {
  // LINT.IfChange
  StrategyDart strategy;
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;
  int64_t keep_alive_interval_millis = 0;
  int64_t keep_alive_timeout_millis = 0;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t is_out_of_band_connection = false;
  const char *fast_advertisement_service_uuid;
  const char *remote_bluetooth_mac_address;

  Mediums mediums;
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
  char *endpoint_info;
  ConnectionListenerDart connection_listener;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_request_info.dart)
};

struct PayloadDart {
  // LINT.IfChange
  int64_t id;
  PayloadType type;
  int64_t size;
  char *data;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload.dart)
};

static void ResultCB(Status status);

static void ListenerInitiatedCB(const char *endpoint_id,
                                const ConnectionResponseInfoW &connection_info);
static void ListenerAcceptedCB(const char *endpoint_id);
static void ListenerRejectedCB(const char *endpoint_id,
                               connections::Status status);
static void ListenerDisconnectedCB(const char *endpoint_id);
static void ListenerBandwidthChangedCB(const char *endpoint_id, MediumW medium);
static void ListenerEndpointFoundCB(const char *endpoint_id,
                                    const char *endpoint_info,
                                    size_t endpoint_info_size,
                                    const char *str_service_id);
static void ListenerEndpointLostCB(const char *endpoint_id);
static void ListenerEndpointDistanceChangedCB(const char *endpoint_id,
                                              DistanceInfoW info);
static void ListenerPayloadCB(const char *endpoint_id, PayloadW &payload);
static void ListenerPayloadProgressCB(const char *endpoint_id,
                                      const PayloadProgressInfoW &info);

DLL_API void __stdcall EnableBleV2Dart(Core *pCore, int64_t enable,
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
DLL_API void __stdcall StartAdvertisingDart(Core *pCore, const char *service_id,
                                            AdvertisingOptionsDart options_dart,
                                            ConnectionRequestInfoDart info_dart,
                                            Dart_Port result_cb);

// Stops advertising a local endpoint. Should be called after calling
// StartAdvertising, as soon as the application no longer needs to advertise
// itself or goes inactive. Payloads can still be sent to connected
// endpoints after advertising ends.
//
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if none of the above errors occurred.
DLL_API void __stdcall StopAdvertisingDart(Core *pCore, Dart_Port result_cb);

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
DLL_API void __stdcall StartDiscoveryDart(Core *pCore, const char *service_id,
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
DLL_API void __stdcall StopDiscoveryDart(Core *pCore, Dart_Port result_cb);

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
DLL_API void __stdcall RequestConnectionDart(
    Core *pCore, const char *endpoint_id, ConnectionOptionsDart options_dart,
    ConnectionRequestInfoDart info_dart, Dart_Port result_cb);

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
DLL_API void __stdcall AcceptConnectionDart(Core *pCore,
                                            const char *endpoint_id,
                                            PayloadListenerDart listener_dart,
                                            Dart_Port result_cb);

DLL_API void __stdcall RejectConnectionDart(Core *pCore,
                                            const char *endpoint_id,
                                            Dart_Port result_cb);

// Disconnects from a remote endpoint. {@link Payload}s can no longer be sent
// to or received from the endpoint after this method is called.
// endpoint_id - The identifier for the remote endpoint to disconnect from.
// result_cb   - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK - finished successfully.
DLL_API void __stdcall DisconnectFromEndpointDart(Core *pCore,
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
DLL_API void __stdcall SendPayloadDart(Core *pCore, const char *endpoint_id,
                                       PayloadDart payload_dart,
                                       Dart_Port result_cb);
}  // extern "C"
}  // namespace nearby::windows

#endif  // LOCATION_NEARBY_CONNECTIONS_DART_CORE_ADAPTER_DART_H_
