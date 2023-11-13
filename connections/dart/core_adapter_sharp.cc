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
using nearby::connections::ConnectionListener;
using nearby::connections::ConnectionRequestInfo;
using nearby::connections::ConnectionResponseInfo;
using nearby::connections::DiscoveryListener;
using nearby::connections::Payload;
using nearby::connections::PayloadListener;
using nearby::connections::PayloadProgressInfo;
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
    NEARBY_LOG(INFO, "Device distance changed: id=%s", endpoint_id.c_str());
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
                           ConnectionInitiatedCallback initiated_callback,
                           AcceptedCallback accepted_callback,
                           RejectedCallback rejected_callback,
                           ConnectionDisconnectedCallback disconnected_callback,
                           BandwidthChangedCallback bandwidth_changed_callback,
                           OperationResultCallback start_advertising_callback) {
  if (pCore == nullptr) {
    start_advertising_callback({.value = Status::Value::kError});
    return;
  }

  ConnectionListener listener;
  listener.initiated_cb = [initiated_callback](
                              const std::string &endpoint_id,
                              const ConnectionResponseInfo &info) {
    NEARBY_LOG(INFO, "Advertising initiated: id=%s", endpoint_id.c_str());

    // Allocate memory to marshal endpoint_info to managed code.
    // It will be freed on the managed side.
    int size = info.remote_endpoint_info.size();
    char *data = (char *)CoTaskMemAlloc(size);
    memcpy(data, info.remote_endpoint_info.data(), size);

    initiated_callback(endpoint_id.c_str(), data, size);
  };
  listener.accepted_cb = [accepted_callback](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Advertising accepted: id=%s", endpoint_id.c_str());
    accepted_callback(endpoint_id.c_str());
  };
  listener.rejected_cb = [rejected_callback](const std::string &endpoint_id,
                                             Status status) {
    NEARBY_LOG(INFO, "Advertising rejected: id=%s, status: %s",
               endpoint_id.c_str(), status.ToString());
    rejected_callback(endpoint_id.c_str(), status);
  };
  listener.disconnected_cb =
      [disconnected_callback](const std::string &endpoint_id) {
        NEARBY_LOG(INFO, "Advertising disconnected: id=%s", endpoint_id);
        disconnected_callback(endpoint_id.c_str());
      };
  listener.bandwidth_changed_cb = [bandwidth_changed_callback](
                                      const std::string &endpoint_id,
                                      Medium medium) {
    NEARBY_LOG(INFO, "Advertising bandwidth changed: id=%s", endpoint_id);
    bandwidth_changed_callback(endpoint_id.c_str(), medium);
  };

  ConnectionRequestInfo info;
  info.endpoint_info = ByteArray(endpoint_info);
  info.listener = std::move(listener);

  ResultCallback result_callback = [start_advertising_callback](Status status) {
    start_advertising_callback(status);
  };

  pCore->StartAdvertising(service_id, advertising_options, info,
                          std::move(result_callback));
}

void RequestConnectionSharp(
    Core *pCore, const char *endpoint_id, ConnectionOptions connection_options,
    const char *endpoint_info, ConnectionInitiatedCallback initiated_callback,
    AcceptedCallback accepted_callback, RejectedCallback rejected_callback,
    ConnectionDisconnectedCallback disconnected_callback,
    BandwidthChangedCallback bandwidth_changed_callback,
    OperationResultCallback request_connection_callback) {
  int offset1 = offsetof(ConnectionOptions, remote_bluetooth_mac_address);
  int offset2 = offsetof(ConnectionOptions, fast_advertisement_service_uuid);
  int offset3 = offsetof(ConnectionOptions, keep_alive_interval_millis);
  int offset4 = offsetof(ConnectionOptions, connection_info);

  if (pCore == nullptr) {
    request_connection_callback({.value = Status::Value::kError});
    return;
  }

  ConnectionListener listener;
  listener.initiated_cb = [initiated_callback](
                              const std::string &endpoint_id,
                              const ConnectionResponseInfo &info) {
    NEARBY_LOG(INFO, "Connection initiated: id=%s", endpoint_id.c_str());

    // Allocate memory to marshal endpoint_info to managed code.
    // It will be freed on the managed side.
    int size = info.remote_endpoint_info.size();
    char *data = (char *)CoTaskMemAlloc(size);
    memcpy(data, info.remote_endpoint_info.data(), size);

    initiated_callback(endpoint_id.c_str(), data, size);
  };
  listener.accepted_cb = [accepted_callback](const std::string &endpoint_id) {
    NEARBY_LOG(INFO, "Connection accepted: id=%s", endpoint_id.c_str());
    accepted_callback(endpoint_id.c_str());
  };
  listener.rejected_cb = [rejected_callback](const std::string &endpoint_id,
                                             Status status) {
    NEARBY_LOG(INFO, "Connection rejected: id=%s, status: %s",
               endpoint_id.c_str(), status.ToString());
    rejected_callback(endpoint_id.c_str(), status);
  };
  listener.disconnected_cb =
      [disconnected_callback](const std::string &endpoint_id) {
        NEARBY_LOG(INFO, "Connection disconnected: id=%s", endpoint_id);
        disconnected_callback(endpoint_id.c_str());
      };
  listener.bandwidth_changed_cb = [bandwidth_changed_callback](
                                      const std::string &endpoint_id,
                                      Medium medium) {
    NEARBY_LOG(INFO, "Connection bandwidth changed: id=%s", endpoint_id);
    bandwidth_changed_callback(endpoint_id.c_str(), medium);
  };

  ConnectionRequestInfo info;
  info.endpoint_info = ByteArray(endpoint_info);
  info.listener = std::move(listener);

  ResultCallback result_callback =
      [request_connection_callback](Status status) {
        request_connection_callback(status);
      };

  connection_options.remote_bluetooth_mac_address = ByteArray("");
  pCore->RequestConnection(endpoint_id, info, connection_options,
                           std::move(result_callback));
}

void AcceptConnectionSharp(Core *pCore, const char *endpoint_id,
                           PayloadInitiatedCallback payload_initiated_callback,
                           PayloadProgressCallback payload_progress_callback,
                           OperationResultCallback accept_connection_callback) {
  PayloadListener listener;
  listener.payload_cb = [payload_initiated_callback](
                            absl::string_view endpoint_id, Payload paylod) {
    NEARBY_LOG(INFO, "Payload initiated: id=%s", endpoint_id);
    payload_initiated_callback(endpoint_id.data());
  };
  listener.payload_progress_cb =
      [payload_progress_callback](absl::string_view endpoint_id,
                                  PayloadProgressInfo payload_progress_info) {
        NEARBY_LOG(INFO, "Payload progress: id=%s", endpoint_id);
        payload_progress_callback(endpoint_id.data());
      };

  ResultCallback result_callback = [accept_connection_callback](Status status) {
    accept_connection_callback(status);
  };

  pCore->AcceptConnection(endpoint_id, std::move(listener),
                          std::move(result_callback));
}

}  // namespace nearby::windows
