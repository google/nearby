// Copyright 2021-2023 Google LLC
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

#include "connections/dart/core_adapter_sharp.h"

#include <combaseapi.h>

#include <cstdint>
#include <string>

#include "connections/core.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/payload.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

using nearby::connections::BooleanMediumSelector;
using nearby::connections::ConnectionRequestInfo;
using nearby::connections::DistanceInfo;
using nearby::connections::ResultCallback;
using nearby::connections::Strategy;

void StartDiscoverySharp(
    Core *pCore, const char *service_id, DiscoveryOptions discovery_options,
    EndpointFoundCallback endpoint_found_callback,
    EndpointLostCallback endpoint_lost_callback,
    EndpointDistanceChangedCallback endpoint_distance_changed_callback,
    OperationResultCallback start_discovery_callback) {
  if (pCore == nullptr) {
    start_discovery_callback({.value = Status::Value::kError});
    return;
  }

  DiscoveryListener listener;
  listener.endpoint_found_cb = [endpoint_found_callback](
                                   const std::string &endpoint_id,
                                   const ByteArray &endpoint_info,
                                   const std::string &service_id) {
    NEARBY_LOG(INFO, "Device discovered: id=%s", endpoint_id);
    NEARBY_LOG(INFO, "Device discovered: service_id=%s", service_id);
    NEARBY_LOG(INFO, "Device discovered: info=%s", endpoint_info);

    // Allocate memory to marshal endpoint_info to managed code.
    // It will be freed on the managed side.
    int size = endpoint_info.size();
    char *data = (char *)CoTaskMemAlloc(size);
    memcpy(data, endpoint_info.data(), size);

    endpoint_found_callback(endpoint_id.c_str(), data, size,
                            service_id.c_str());
  };

  listener.endpoint_lost_cb =
      [endpoint_lost_callback](const std::string &endpoint_id) {
        NEARBY_LOG(INFO, "Device lost: id=%s", endpoint_id);
        endpoint_lost_callback(endpoint_id.c_str());
      };

  listener.endpoint_distance_changed_cb = [endpoint_distance_changed_callback](
                                              const std::string &endpoint_id,
                                              DistanceInfo distance_info) {
    NEARBY_LOG(INFO, "Device distance changed: id=%s", endpoint_id);
    endpoint_distance_changed_callback(endpoint_id.c_str(), distance_info);
  };

  ResultCallback result_callback = [start_discovery_callback](Status status) {
    start_discovery_callback(status);
  };

  pCore->StartDiscovery(service_id, discovery_options, std::move(listener),
                        std::move(result_callback));
}

void StartAdvertisingSharp(Core *pCore, const char *service_id,
                           AdvertisingOptions advertising_options,
                           const char *endpoint_info,
                           InitiatedCallback initiated_callback,
                           OperationResultCallback start_advertising_callback) {
  if (pCore == nullptr) {
    start_advertising_callback({.value = Status::Value::kError});
    return;
  }

  ConnectionListener listener;
  listener.initiated_cb =
      [initiated_callback](const std::string &endpoint_id,
                           const ConnectionResponseInfo &info) {
        NEARBY_LOG(INFO, "Advertising initiated: id=%s", endpoint_id);
        initiated_callback(endpoint_id.c_str(), info);
      }

  ConnectionRequestInfo info;
  info.endpoint_info = ByteArray(endpoint_info);
  info.listener = std::move(listener);

  ResultCallback result_callback = [start_advertising_callback](Status status) {
    start_advertising_callback(status);
  };

  pCore->StartAdvertising(service_id, advertising_options, info,
                          std::move(result_callback));
}
}  // namespace nearby::windows

// endpoint_found_callback("endpoint id", "info", 4, "service id");

//   int offset1 = offsetof(DiscoveryOptions, auto_upgrade_bandwidth);
//   int offset2 = offsetof(DiscoveryOptions, enforce_topology_constraints);
//   int offset3 = offsetof(DiscoveryOptions, is_out_of_band_connection);
//   int offset4 = offsetof(DiscoveryOptions,
//   fast_advertisement_service_uuid); int offset5 =
//   offsetof(DiscoveryOptions, low_power);

//   int offset6 = offsetof(DiscoveryOptions, strategy);
//   int offset7 = offsetof(DiscoveryOptions, allowed);

//   // int offset8 = offsetof(Strategy, connection_type_);
//   // int offset9 = offsetof(Strategy, topology_type_);

//   int offset10 = offsetof(BooleanMediumSelector, bluetooth);
//   int offset11 = offsetof(BooleanMediumSelector, ble);
//   int offset12 = offsetof(BooleanMediumSelector, web_rtc);
//   int offset13 = offsetof(BooleanMediumSelector, wifi_lan);
//   int offset14 = offsetof(BooleanMediumSelector, wifi_hotspot);
//   int offset15 = offsetof(BooleanMediumSelector, wifi_direct);

//   std::cout << offset1 << ' ' << offset2 << ' ' << offset3 << ' ' <<
//   offset4
//             << ' ' << offset5 << ' ' << offset6 << '  ' << offset7;

// current_discovery_listener_sharp.endpoint_found_callback =
//     endpoint_found_callback;
// current_discovery_listener_sharp.endpoint_lost_callback =
//     endpoint_lost_callback;
// current_discovery_listener_sharp.endpoint_distance_changed_callback =
//     endpoint_distance_changed_callback;

// DiscoveryListenerW listener(EndpointFoundStub, EndpointLostStub,
//                             EndpointDistanceChangedStub);
