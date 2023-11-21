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
using nearby::connections::ConnectionOptions;
using nearby::connections::DiscoveryOptions;
using nearby::connections::DistanceInfo;
using nearby::connections::Medium;
using nearby::connections::PayloadProgressInfo;
using nearby::connections::PayloadType;
using nearby::connections::ResultCallback;

typedef void OperationResultCallback(Status);

typedef void EndpointFoundCallback(const char *endpoint_id,
                                   const char *endpoint_info,
                                   size_t endpoint_info_size,
                                   const char *service_id);
typedef void EndpointLostCallback(const char *endpoint_id);
typedef void EndpointDistanceChangedCallback(const char *endpoint_id,
                                             DistanceInfo distance_info);
typedef void ConnectionInitiatedCallback(const char *endpoint_id,
                                         const char *endpoint_info,
                                         size_t endpoint_info_size);
typedef void AcceptedCallback(const char *endpoint_id);
typedef void RejectedCallback(const char *endpoint_id, Status status);
typedef void ConnectionDisconnectedCallback(const char *endpoint_id);
typedef void BandwidthChangedCallback(const char *endpoint_id, Medium medium);
typedef void PayloadInitiatedCallback(const char *endpoint_id,
                                      PayloadId payload_id, int payload_size,
                                      const char *payload_content);
typedef void PayloadProgressCallback(const char *endpoint_id,
                                     int64_t payload_id,
                                     PayloadProgressInfo::Status payload_status,
                                     int64_t total_bytes,
                                     int64_t bytes_transferred);

extern "C" {
DLL_API Status __stdcall StartDiscoveringSharp(
    Core *pCore, const char *service_id, DiscoveryOptions discovery_options,
    EndpointFoundCallback endpoint_found_callback,
    EndpointLostCallback endpoint_lost_callback,
    EndpointDistanceChangedCallback endpoint_distance_changed_callback);

DLL_API Status __stdcall StopDiscoveringSharp(Core *pCore);

DLL_API Status __stdcall StartAdvertisingSharp(
    Core *pCore, const char *service_id, AdvertisingOptions advertising_options,
    const char *endpoint_info, ConnectionInitiatedCallback initiated_callback,
    AcceptedCallback accepted_callback, RejectedCallback rejected_callback,
    ConnectionDisconnectedCallback disconnected_callback,
    BandwidthChangedCallback bandwidth_changed_callback);

DLL_API Status __stdcall StopAdvertisingSharp(Core *pCore);

DLL_API Status __stdcall RequestConnectionSharp(
    Core *pCore, const char *endpoint_id, ConnectionOptions connection_options,
    const char *endpoint_info, ConnectionInitiatedCallback initiated_callback,
    AcceptedCallback accepted_callback, RejectedCallback rejected_callback,
    ConnectionDisconnectedCallback disconnected_callback,
    BandwidthChangedCallback bandwidth_changed_callback);

DLL_API Status __stdcall AcceptConnectionSharp(
    Core *pCore, const char *endpoint_id,
    PayloadInitiatedCallback payload_initiated_callback,
    PayloadProgressCallback payload_progress_callback);

DLL_API Status __stdcall SendPayloadBytesSharp(Core *pCore,
                                               const char *endpoint_id,
                                               size_t payload_size,
                                               const char *payload_content);

DLL_API Status __stdcall DisconnectSharp(Core *pCore, const char *endpoint_id);

DLL_API const char *GetLocalEndpointIdSharp(Core *pCore);
}
}  // namespace nearby::windows

#endif