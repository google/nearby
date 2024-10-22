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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_TYPES_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef void* NC_INSTANCE;

typedef int64_t NC_PAYLOAD_ID;

// NC_DATA is used to define a byte array. Its last byte is not zero.
typedef struct NC_DATA {
  int64_t size;
  char* data;
} NC_DATA, *PNC_DATA;

// Supported mediums by Nearby Connections.
typedef enum NC_MEDIUM {
  NC_MEDIUM_UNKNOWN = 0,
  NC_MEDIUM_MDNS = 1,  // deprecated
  NC_MEDIUM_BLUETOOTH = 2,
  NC_MEDIUM_WIFI_HOTSPOT = 3,
  NC_MEDIUM_BLE = 4,
  NC_MEDIUM_WIFI_LAN = 5,
  NC_MEDIUM_WIFI_AWARE = 6,
  NC_MEDIUM_NFC = 7,
  NC_MEDIUM_WIFI_DIRECT = 8,
  NC_MEDIUM_WEB_RTC = 9,
  NC_MEDIUM_BLE_L2CAP = 10,
  NC_MEDIUM_USB = 11,
  NC_MEDIUM_MAX = 12
} NC_MEDIUM;

typedef enum NC_CONNECTION_TYPE {
  NC_CONNECTION_TYPE_NONE = 0,
  NC_CONNECTION_TYPE_POINT_TO_POINT = 1,
} NC_CONNECTION_TYPE;

typedef enum NC_TOPOLOGY_TYPE {
  NC_TOPOLOGY_TYPE_UNKNOWN = 0,
  NC_TOPOLOGY_TYPE_ONE_TO_ONE = 1,
  NC_TOPOLOGY_TYPE_ONE_TO_MANY = 2,
  NC_TOPOLOGY_TYPE_MANY_TO_MANY = 3,
} NC_TOPOLOGY_TYPE;

typedef enum NC_STRATEGY_TYPE {
  NC_STRATEGY_TYPE_NONE,
  NC_STRATEGY_TYPE_P2P_CLUSTER,
  NC_STRATEGY_TYPE_P2P_STAR,
  NC_STRATEGY_TYPE_P2P_POINT_TO_POINT
} NC_STRATEGY_TYPE;

typedef enum NC_DISTANCE_INFO {
  NC_DISTANCE_INFO_UNKNOWN = 1,
  NC_DISTANCE_INFO_VERYCLOSE = 2,
  NC_DISTANCE_INFO_CLOSE = 3,
  NC_DISTANCE_INFO_FAR = 4,
} NC_DISTANCE_INFO;

typedef enum NC_STATUS {
  NC_STATUS_SUCCESS,
  NC_STATUS_ERROR,
  NC_STATUS_OUTOFORDERAPICALL,
  NC_STATUS_ALREADYHAVEACTIVESTRATEGY,
  NC_STATUS_ALREADYADVERTISING,
  NC_STATUS_ALREADYDISCOVERING,
  NC_STATUS_ALREADYLISTENING,
  NC_STATUS_ENDPOINTIOERROR,
  NC_STATUS_ENDPOINTUNKNOWN,
  NC_STATUS_CONNECTIONREJECTED,
  NC_STATUS_ALREADYCONNECTEDTOENDPOINT,
  NC_STATUS_NOTCONNECTEDTOENDPOINT,
  NC_STATUS_BLUETOOTHERROR,
  NC_STATUS_BLEERROR,
  NC_STATUS_WIFILANERROR,
  NC_STATUS_PAYLOADUNKNOWN,
  NC_STATUS_RESET,
  NC_STATUS_TIMEOUT,
  NC_STATUS_UNKNOWN,
  NC_STATUS_NEXTVALUE,
} NC_STATUS;

typedef enum NC_PAYLOAD_TYPE {
  NC_PAYLOAD_TYPE_UNKNOWN = 0,
  NC_PAYLOAD_TYPE_BYTES = 1,
  NC_PAYLOAD_TYPE_STREAM = 2,
  NC_PAYLOAD_TYPE_FILE = 3
} NC_PAYLOAD_TYPE;

typedef enum NC_PAYLOAD_DIRECTION {
  NC_PAYLOAD_DIRECTION_UNKNOWN = 0,
  NC_PAYLOAD_DIRECTION_INCOMING = 1,
  NC_PAYLOAD_DIRECTION_OUTGOING = 2,
} NC_PAYLOAD_DIRECTION;

typedef enum NC_IO_EXCEPTION {
  NC_IO_EXCEPTION_FAILED = -1,
  NC_IO_EXCEPTION_SUCCESS = 0,
  NC_IO_EXCEPTION_IO = 1,
  NC_IO_EXCEPTION_INTERRUPTED = 2,
  NC_IO_EXCEPTION_INVALID_PROTOCOL_BUFFER = 3,
  NC_IO_EXCEPTION_EXECUTION = 4,
  NC_IO_EXCEPTION_TIMEOUT = 5,
  NC_IO_EXCEPTION_ILLEGALCHARACTERS = 6,
} NC_IO_EXCEPTION;

// Defines struct types in Nearby connections.

typedef struct NC_STRATEGY {
  NC_STRATEGY_TYPE type;
  NC_CONNECTION_TYPE connection_type;
  NC_TOPOLOGY_TYPE topology_type;
} NC_STRATEGY;

typedef struct NC_COMMON_OPTIONS {
  NC_STRATEGY strategy;
  bool allowed_mediums[NC_MEDIUM_MAX];
} NC_COMMON_OPTIONS;

typedef struct NC_ADVERTISING_OPTIONS {
  NC_COMMON_OPTIONS common_options;
  bool auto_upgrade_bandwidth;
  bool enforce_topology_constraints;
  bool enable_bluetooth_listening;
  bool enable_webrtc_listening;
  bool low_power;
  bool is_out_of_band_connection;
  NC_DATA fast_advertisement_service_uuid;
  NC_DATA device_info;
} NC_ADVERTISING_OPTIONS, *PNC_ADVERTISING_OPTIONS;

typedef struct NC_CONNECTION_OPTIONS {
  NC_COMMON_OPTIONS common_options;
  bool auto_upgrade_bandwidth;
  bool enforce_topology_constraints;
  bool low_power;
  bool is_out_of_band_connection;
  NC_DATA remote_bluetooth_mac_address;
  NC_DATA fast_advertisement_service_uuid;
  int keep_alive_interval_millis;
  int keep_alive_timeout_millis;
} NC_CONNECTION_OPTIONS, *PNC_CONNECTION_OPTIONS;

typedef struct NC_DISCOVERY_OPTIONS {
  NC_COMMON_OPTIONS common_options;
  bool auto_upgrade_bandwidth;
  bool enforce_topology_constraints;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection;
  NC_DATA fast_advertisement_service_uuid;
  bool low_power;
} NC_DISCOVERY_OPTIONS, *PNC_DISCOVERY_OPTIONS;

typedef struct NC_CONNECTION_RESPONSE_INFO {
  NC_DATA remote_endpoint_info;
  NC_DATA authentication_token;
  NC_DATA raw_authentication_token;
  bool is_incoming_connection;
  bool is_connection_verified;
} NC_CONNECTION_RESPONSE_INFO, *PNC_CONNECTION_RESPONSE_INFO;

// Defines callbacks in Nearby Connections.

typedef void (*NcCallbackResult)(NC_STATUS status);

typedef void (*NcCallbackConnectionInitiated)(
    NC_INSTANCE instance, int endpoint_id,
    const NC_CONNECTION_RESPONSE_INFO* info);
typedef void (*NcCallbackConnectionAccepted)(NC_INSTANCE instance,
                                             int endpoint_id);
typedef void (*NcCallbackConnectionRejected)(NC_INSTANCE instance,
                                             int endpoint_id, NC_STATUS status);
typedef void (*NcCallbackConnectionDisconnected)(NC_INSTANCE instance,
                                                 int endpoint_id);
typedef void (*NcCallbackConnectionBandwidthChanged)(NC_INSTANCE instance,
                                                     int endpoint_id,
                                                     NC_MEDIUM medium);

typedef struct NC_CONNECTION_REQUEST_INFO {
  NC_DATA endpoint_info;
  NcCallbackConnectionInitiated initiated_callback;
  NcCallbackConnectionAccepted accepted_callback;
  NcCallbackConnectionRejected rejected_callback;
  NcCallbackConnectionDisconnected disconnected_callback;
  NcCallbackConnectionBandwidthChanged bandwidth_changed_callback;
} NC_CONNECTION_REQUEST_INFO;

typedef void (*NcCallbackDiscoveryEndpointFound)(NC_INSTANCE instance,
                                                 int endpoint_id,
                                                 const NC_DATA* endpoint_info,
                                                 const NC_DATA* service_id);
typedef void (*NcCallbackDiscoveryEndpointLost)(NC_INSTANCE instance,
                                                int endpoint_id);
typedef void (*NcCallbackDiscoveryEndpointDistanceChanged)(
    NC_INSTANCE instance, int endpoint_id, NC_DISTANCE_INFO info);

typedef struct NC_DISCOVERY_LISTENER {
  NcCallbackDiscoveryEndpointFound endpoint_found_callback;
  NcCallbackDiscoveryEndpointLost endpoint_lost_callback;
  NcCallbackDiscoveryEndpointDistanceChanged endpoint_distance_changed_callback;
} NC_DISCOVERY_LISTENER;

typedef struct NC_BYTES_PAYLOAD {
  NC_DATA content;
} NC_BYTES_PAYLOAD;

typedef int (*NcCallbackStreamRead)(NC_INSTANCE stream, char* buffer,
                                    int64_t size);
typedef int (*NcCallbackStreamClose)(NC_INSTANCE stream);
typedef int (*NcCallbackStreamSkip)(NC_INSTANCE stream, int64_t skip);

typedef struct NC_STREAM_PAYLOAD {
  NC_INSTANCE stream;
  NcCallbackStreamRead read_callback;
  NcCallbackStreamSkip skip_callback;
  NcCallbackStreamClose close_callback;
} NC_STREAM_PAYLOAD;

typedef struct NC_FILE_PAYLOAD {
  int64_t offset;
  char* file_name;
  char* parent_folder;
} NC_FILE_PAYLOAD;

typedef union NC_PAYLOAD_CONTENT {
  NC_BYTES_PAYLOAD bytes;
  NC_STREAM_PAYLOAD stream;
  NC_FILE_PAYLOAD file;
} NC_PAYLOAD_CONTENT;

typedef struct NC_PAYLOAD {
  NC_PAYLOAD_ID id;
  NC_PAYLOAD_TYPE type;
  NC_PAYLOAD_DIRECTION direction;
  NC_PAYLOAD_CONTENT content;
} NC_PAYLOAD;

typedef enum NC_PAYLOAD_PROGRESS_INFO_STATUS {
  NC_PAYLOAD_PROGRESS_INFO_STATUS_SUCCESS,
  NC_PAYLOAD_PROGRESS_INFO_STATUS_FAILURE,
  NC_PAYLOAD_PROGRESS_INFO_STATUS_INPROGRESS,
  NC_PAYLOAD_PROGRESS_INFO_STATUS_CANCELED,
} NC_PAYLOAD_PROGRESS_INFO_STATUS;

typedef struct NC_PAYLOAD_PROGRESS_INFO {
  NC_PAYLOAD_ID id;
  NC_PAYLOAD_PROGRESS_INFO_STATUS status;
  size_t total_bytes;
  size_t bytes_transferred;
} NC_PAYLOAD_PROGRESS_INFO;

typedef void (*NcCallbackPayloadReceived)(NC_INSTANCE instance, int endpoint_id,
                                          const NC_PAYLOAD* payload);
typedef void (*NcCallbackPayloadProgressUpdated)(
    NC_INSTANCE instance, int endpoint_id,
    const NC_PAYLOAD_PROGRESS_INFO* info);

typedef struct NC_PAYLOAD_LISTENER {
  NcCallbackPayloadReceived received_callback;
  NcCallbackPayloadProgressUpdated progress_updated_callback;
} NC_PAYLOAD_LISTENER;

typedef struct NC_OUT_OF_BAND_CONNECTION_METADATA {
  // Medium to use for the out-of-band connection.
  NC_MEDIUM medium;

  // Endpoint ID to use for the injected connection; will be included in the
  // endpoint_found_cb callback. Must be exactly 4 bytes and should be randomly-
  // generated such that no two IDs are identical.
  int endpoint_id;

  // Endpoint info to use for the injected connection; will be included in the
  // endpoint_found_cb callback. Should uniquely identify the InjectEndpoint()
  // call so that the client which made the call can verify the endpoint
  // that was found is the one that was injected.
  //
  // Cannot be empty, and must be <131 bytes.
  NC_DATA endpoint_info;

  // Used for Bluetooth connections.
  NC_DATA remote_bluetooth_mac_address;
} NC_OUT_OF_BAND_CONNECTION_METADATA, *PNC_OUT_OF_BAND_CONNECTION_METADATA;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_NC_TYPES_H_
