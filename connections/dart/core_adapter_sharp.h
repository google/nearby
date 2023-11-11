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

#ifndef LOCATION_NEARBY_CONNECTIONS_SHARP_CORE_ADAPTER_SHARP_H_
#define LOCATION_NEARBY_CONNECTIONS_SHARP_CORE_ADAPTER_SHARP_H_

#include "connections/advertising_options.h"
#include "connections/c/core_adapter.h"
#include "connections/discovery_options.h"
#include "connections/listeners.h"

namespace nearby::windows {

using nearby::connections::AdvertisingOptions;
using nearby::connections::ConnectionListener;
using nearby::connections::ConnectionResponseInfo;
using nearby::connections::DiscoveryListener;
using nearby::connections::DiscoveryOptions;
using nearby::connections::DistanceInfo;
using nearby::connections::Medium;

typedef void OperationResultCallback(Status);

// Types for discovery
typedef void EndpointFoundCallback(const char *endpoint_id,
                                   const char *endpoint_info,
                                   size_t endpoint_info_size,
                                   const char *service_id);
typedef void EndpointLostCallback(const char *endpoint_id);
typedef void EndpointDistanceChangedCallback(const char *endpoint_id,
                                             DistanceInfo distance_info);

struct DiscoveryListenerSharp {
  EndpointFoundCallback *endpoint_found_callback;
  EndpointLostCallback *endpoint_lost_callback;
  EndpointDistanceChangedCallback *endpoint_distance_changed_callback;
};

// Types for advertising and connecting
typedef void InitiatedCallback(const char *endpoint_id,
                               const ConnectionResponseInfo &info);
typedef void AcceptedCallback(const char *endpoint_id);
typedef void RejectedCallback(const char *endpoint_id, Status status);
typedef void DisconnectedCallback(const char *endpoint_id);
typedef void BandwidthChangedCallback(const char *endpoint_id, Medium medium);

extern "C" {
DLL_API void __stdcall StartDiscoverySharp(
    Core *pCore, const char *service_id, DiscoveryOptions discovery_options,
    EndpointFoundCallback endpoint_found_callback,
    EndpointLostCallback endpoint_lost_callback,
    EndpointDistanceChangedCallback endpoint_distance_changed_callback,
    OperationResultCallback start_discovery_callback);

DLL_API void __stdcall StartAdvertisingSharp(
    Core *pCore, const char *service_id, AdvertisingOptions advertising_options,
    const char *endpoint_info,
    OperationResultCallback start_advertising_callback);
}

}  // namespace nearby::windows

#endif