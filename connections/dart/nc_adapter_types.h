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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_TYPES_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_TYPES_H_

#include <cstdint>

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

struct DataDart {
  // LINT.IfChange
  int64_t size;
  char *data;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/data.dart)
};

struct AdvertisingOptionsDart {
  // LINT.IfChange
  StrategyDart strategy;
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;
  int64_t low_power;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  int64_t is_out_of_band_connection = false;
  DataDart fast_advertisement_service_uuid;

  // The information about this device (eg. name, device type),
  // to appear on the remote device.
  // Defined by client/application.
  DataDart device_info;

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
  DataDart remote_bluetooth_mac_address;
  DataDart fast_advertisement_service_uuid;
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
  DataDart fast_advertisement_service_uuid;
  DataDart remote_bluetooth_mac_address;
  int64_t low_power;
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
  DataDart endpoint_info;
  ConnectionListenerDart connection_listener;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/connection_request_info.dart)
};

struct PayloadDart {
  // LINT.IfChange
  int64_t id;
  PayloadTypeDart type;
  int64_t size;
  DataDart data;
  // LINT.ThenChange(//depot/google3/location/nearby/apps/helloconnections/plugins/nearby_connections/platform/lib/types/payload.dart)
};

#ifdef __cplusplus
}
#endif

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DART_NC_ADAPTER_TYPES_H_
