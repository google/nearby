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

#ifndef LOCATION_NEARBY_CONNECTIONS_WINDOWS_DART_CORE_ADAPTER_DART_H_
#define LOCATION_NEARBY_CONNECTIONS_WINDOWS_DART_CORE_ADAPTER_DART_H_

#include "third_party/nearby_connections/windows/core_adapter.h"

namespace location {
namespace nearby {
namespace connections {

enum StrategyDart {
  P2P_CLUSTER = 0,
  P2P_STAR,
  P2P_POINT_TO_POINT,
};

struct ConnectionOptionsDart {
  StrategyDart strategy;
  int64_t auto_upgrade_bandwidth;
  int64_t enforce_topology_constraints;
  int64_t enable_bluetooth;
  int64_t enable_ble;
  int64_t advertise_nearby_notifications_beacon;
  int64_t use_low_power_mode;
  int64_t discover_fast_advertisements;
  int64_t enable_wifi_lan;
  int64_t enable_nfc;
  int64_t enable_wifi_aware;
  int64_t enable_web_rtc;
};

struct ConnectionRequestInfoDart {
  char* endpoint_info;
  int64_t initiated_cb;
  int64_t accepted_cb;
  int64_t rejected_cb;
  int64_t disconnected_cb;
  int64_t bandwidth_changed_cb;
};

struct DiscoveryListenerDart {
  int64_t found_cb;
  int64_t lost_cb;
  int64_t distance_changed_cb;
};

struct PayloadListenerDart {
  int64_t payload_cb;
  int64_t payload_progress_cb;
};

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
DLL_EXPORT void __stdcall StartAdvertisingDart(Core* pCore,
                                   const char* service_id,
                                   ConnectionOptionsDart options_dart,
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
DLL_EXPORT void __stdcall StopAdvertisingDart(Core* pCore,
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
DLL_EXPORT void __stdcall StartDiscoveryDart(Core* pCore,
                                    const char* service_id,
                                    ConnectionOptionsDart options_dart,
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
DLL_EXPORT void __stdcall StopDiscoveryDart(Core* pCore,
                                         Dart_Port result_cb);
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // LOCATION_NEARBY_CONNECTIONS_WINDOWS_CORE_ADAPTER_DART_H_
