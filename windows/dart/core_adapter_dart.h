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

// Starts advertising an endpoint for a local app.
//
// service_id - An identifier to advertise your app to other endpoints.
//              This can be an arbitrary string, so long as it uniquely
//              identifies your service. A good default is to use your
//              app's package name.
// endpoint_info - A human readable name for this endpoint, to appear on
//                 other devices.
// initiated_cb, accepted_cb, rejected_cb, disconnected_cb,
//              bandwidth_changed_cb - A callback notified when remote
//              endpoints request a connection to this endpoint.
// result_cb - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if advertising started successfully.
//     Status::STATUS_ALREADY_ADVERTISING if the app is already advertising.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DLL_EXPORT void __stdcall StartAdvertisingDart(Core* pCore,
                                      const char* service_id,
                                      StrategyDart strategy,
                                      ByteArray endpoint_info,
                                      int auto_upgrade_bandwidth,
                                      int enforce_topology_constraints,
                                      int enable_bluetooth,
                                      int enable_ble,
                                      int advertise_nearby_notifications_beacon,
                                      int use_low_power_mode,
                                      int discover_fast_advertisements,
                                      int enable_wifi_lan,
                                      int enable_nfc,
                                      int enable_wifi_aware,
                                      int enable_web_rtc,
                                      Dart_Port initiated_cb,
                                      Dart_Port accepted_cb,
                                      Dart_Port rejected_cb,
                                      Dart_Port disconnected_cb,
                                      Dart_Port bandwidth_changed_cb,
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
// found_cb, lost_cb, distance_changed_cb - A callback notified when a remote
//              endpoint is discovered.
// result_cb  - to access the status of the operation when available.
//   Possible status codes include:
//     Status::STATUS_OK if discovery started successfully.
//     Status::STATUS_ALREADY_DISCOVERING if the app is already
//         discovering the specified service.
//     Status::STATUS_OUT_OF_ORDER_API_CALL if the app is currently
//         connected to remote endpoints; call StopAllEndpoints first.
DLL_EXPORT void __stdcall StartDiscoveryDart(Core* pCore,
                                    const char* service_id,
                                    StrategyDart strategy,
                                    int forward_unrecognized_bluetooth_devices,
                                    int enable_bluetooth,
                                    int enable_ble,
                                    int use_low_power_mode,
                                    int discover_fast_advertisements,
                                    int enable_wifi_lan,
                                    int enable_nfc,
                                    int enable_wifi_aware,
                                    Dart_Port found_cb,
                                    Dart_Port lost_cb,
                                    Dart_Port distance_changed_cb,
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
