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

#include "absl/synchronization/notification.h"
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

Status StartDiscoveringSharp(
    Core *pCore, const char *service_id, DiscoveryOptions discovery_options,
    EndpointFoundCallback endpoint_found_callback,
    EndpointLostCallback endpoint_lost_callback,
    EndpointDistanceChangedCallback endpoint_distance_changed_callback) {
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

  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };

  pCore->StartDiscovery(service_id, discovery_options, std::move(listener),
                        std::move(result_callback));
  done.WaitForNotification();
  return result;
}

Status StopDiscoveringSharp(Core *pCore) {
  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->StopDiscovery(std::move(result_callback));
  done.WaitForNotification();
  return result;
}

Status StartAdvertisingSharp(
    Core *pCore, const char *service_id, AdvertisingOptions advertising_options,
    const char *endpoint_info, ConnectionInitiatedCallback initiated_callback,
    AcceptedCallback accepted_callback, RejectedCallback rejected_callback,
    ConnectionDisconnectedCallback disconnected_callback,
    BandwidthChangedCallback bandwidth_changed_callback) {
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

  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->StartAdvertising(service_id, advertising_options, info,
                          std::move(result_callback));
  done.WaitForNotification();
  return result;
}

Status StopAdvertisingSharp(Core *pCore) {
  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->StopAdvertising(std::move(result_callback));
  done.WaitForNotification();
  return result;
}

Status RequestConnectionSharp(
    Core *pCore, const char *endpoint_id, ConnectionOptions connection_options,
    const char *endpoint_info, ConnectionInitiatedCallback initiated_callback,
    AcceptedCallback accepted_callback, RejectedCallback rejected_callback,
    ConnectionDisconnectedCallback disconnected_callback,
    BandwidthChangedCallback bandwidth_changed_callback) {
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
  connection_options.remote_bluetooth_mac_address = ByteArray("");

  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->RequestConnection(endpoint_id, info, connection_options,
                           std::move(result_callback));
  done.WaitForNotification();
  return result;
}

Status AcceptConnectionSharp(
    Core *pCore, const char *endpoint_id,
    PayloadInitiatedCallback payload_initiated_callback,
    PayloadProgressCallback payload_progress_callback) {
  PayloadListener listener;
  listener.payload_cb = [payload_initiated_callback](
                            absl::string_view endpoint_id, Payload payload) {
    NEARBY_LOG(INFO, "Payload initiated: id=%s", endpoint_id.data());
    ByteArray bytes = payload.AsBytes();
    payload_initiated_callback(endpoint_id.data(), payload.GetId(),
                               bytes.size(), bytes.data());
  };
  listener.payload_progress_cb =
      [payload_progress_callback](absl::string_view endpoint_id,
                                  PayloadProgressInfo payload_progress_info) {
        NEARBY_LOG(INFO, "Payload progress: id=%s", endpoint_id.data());
        payload_progress_callback(
            endpoint_id.data(), payload_progress_info.payload_id,
            payload_progress_info.status, payload_progress_info.total_bytes,
            payload_progress_info.bytes_transferred);
      };

  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->AcceptConnection(endpoint_id, std::move(listener),
                          std::move(result_callback));
  done.WaitForNotification();
  return result;
}

struct PayloadDeleter {
  void operator()(Payload *p) { delete p; }
};

Status SendPayloadBytesSharp(Core *pCore, const char *endpoint_id,
                             size_t payload_size, const char *payload_content) {
  auto payload_id = nearby::connections::Payload::GenerateId();
  std::unique_ptr<connections::Payload, connections::PayloadDeleter> payload(
      new Payload(payload_id, ByteArray(payload_content, payload_size)));
  std::string endpoint_id_ = std::string(endpoint_id);
  absl::Span<const std::string> span{&endpoint_id_, 1};

  absl::Notification done;
  Status result;
  ResultCallback result_callback = [&done, &result](Status status) {
    result = status;
    done.Notify();
  };
  pCore->SendPayload(span, std::move(*payload), std::move(result_callback));
  done.WaitForNotification();
  return result;
}

}  // namespace nearby::windows
