// Copyright 2022 Google LLC
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

#include "sharing/nearby_connections_service_impl.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/payload.h"
#include "connections/strategy.h"
#include "internal/analytics/event_logger.h"
#include "internal/platform/logging.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {
namespace {

Core* GetService(NearbyConnectionsService::HANDLE handle) {
  return reinterpret_cast<Core*>(handle);
}

}  // namespace

NearbyConnectionsServiceImpl::NearbyConnectionsServiceImpl(
    nearby::analytics::EventLogger* event_logger) {
  static ServiceControllerRouter* router = new ServiceControllerRouter();
  static Core* core = new Core(event_logger, router);
  service_handle_ = core;
}

NearbyConnectionsServiceImpl::~NearbyConnectionsServiceImpl() = default;

void NearbyConnectionsServiceImpl::StartAdvertising(
    absl::string_view service_id, const std::vector<uint8_t>& endpoint_info,
    AdvertisingOptions advertising_options,
    ConnectionListener advertising_listener,
    std::function<void(Status status)> callback) {
  advertising_listener_ = std::move(advertising_listener);

  NcAdvertisingOptions options{};
  options.strategy = ConvertToServiceStrategy(advertising_options.strategy);
  options.allowed.ble = advertising_options.allowed_mediums.ble;
  options.allowed.bluetooth = advertising_options.allowed_mediums.bluetooth;
  options.allowed.web_rtc = advertising_options.allowed_mediums.web_rtc;
  options.allowed.wifi_lan = advertising_options.allowed_mediums.wifi_lan;
  options.auto_upgrade_bandwidth = advertising_options.auto_upgrade_bandwidth;
  options.enforce_topology_constraints =
      advertising_options.enforce_topology_constraints;
  options.enable_bluetooth_listening =
      advertising_options.enable_bluetooth_listening;
  options.enable_webrtc_listening = advertising_options.enable_webrtc_listening;
  options.use_stable_endpoint_id = advertising_options.use_stable_endpoint_id;
  options.fast_advertisement_service_uuid =
      advertising_options.fast_advertisement_service_uuid.uuid;

  NcConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      NcByteArray(std::string(endpoint_info.begin(), endpoint_info.end()));
  connection_request_info.listener.initiated_cb =
      [&](const std::string& endpoint_id,
          const NcConnectionResponseInfo& info) {
        ConnectionInfo connection_info;
        connection_info.authentication_token = info.authentication_token;
        std::string remote_end_point = std::string(info.remote_endpoint_info);
        connection_info.endpoint_info = std::vector<uint8_t>(
            remote_end_point.begin(), remote_end_point.end());
        connection_info.is_incoming_connection = info.is_incoming_connection;
        std::string raw_authentication_token =
            std::string(info.raw_authentication_token);
        connection_info.raw_authentication_token = std::vector<uint8_t>(
            raw_authentication_token.begin(), raw_authentication_token.end());
        advertising_listener_.initiated_cb(endpoint_id, connection_info);
      };
  connection_request_info.listener.accepted_cb =
      [&](const std::string& endpoint_id) {
        advertising_listener_.accepted_cb(endpoint_id);
      };
  connection_request_info.listener.rejected_cb =
      [&](const std::string& endpoint_info, NcStatus status) {
        advertising_listener_.rejected_cb(endpoint_info,
                                          ConvertToStatus(status));
      };
  connection_request_info.listener.disconnected_cb =
      [&](const std::string& endpoint_info) {
        advertising_listener_.disconnected_cb(endpoint_info);
      };
  connection_request_info.listener.bandwidth_changed_cb =
      [&](const std::string& endpoint_id, NcMedium medium) {
        advertising_listener_.bandwidth_changed_cb(endpoint_id,
                                                   static_cast<Medium>(medium));
      };

  GetService(service_handle_)
      ->StartAdvertising(service_id, options,
                         std::move(connection_request_info),
                         BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::StopAdvertising(
    absl::string_view service_id, std::function<void(Status status)> callback) {
  GetService(service_handle_)->StopAdvertising(BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::StartDiscovery(
    absl::string_view service_id, DiscoveryOptions discovery_options,
    DiscoveryListener discovery_listener,
    std::function<void(Status status)> callback) {
  discovery_listener_ = std::move(discovery_listener);

  NcDiscoveryOptions options{};
  options.strategy = ConvertToServiceStrategy(discovery_options.strategy);
  options.allowed.ble = discovery_options.allowed_mediums.ble;
  options.allowed.bluetooth = discovery_options.allowed_mediums.bluetooth;
  options.allowed.web_rtc = discovery_options.allowed_mediums.web_rtc;
  options.allowed.wifi_lan = discovery_options.allowed_mediums.wifi_lan;
  if (discovery_options.fast_advertisement_service_uuid.has_value()) {
    options.fast_advertisement_service_uuid =
        (*discovery_options.fast_advertisement_service_uuid).uuid;
  }

  options.is_out_of_band_connection =
      discovery_options.is_out_of_band_connection;
  NcDiscoveryListener listener;
  listener.endpoint_found_cb = [this](const std::string& endpoint_id,
                                      const NcByteArray& endpoint_info,
                                      const std::string& service_id) {
    std::string endpoint_info_data = std::string(endpoint_info);
    discovery_listener_.endpoint_found_cb(
        endpoint_id,
        DiscoveredEndpointInfo(std::vector<uint8_t>(endpoint_info_data.begin(),
                                                    endpoint_info_data.end()),
                               service_id));
  };
  listener.endpoint_lost_cb = [this](const std::string& endpoint_id) {
    discovery_listener_.endpoint_lost_cb(endpoint_id);
  };
  listener.endpoint_distance_changed_cb = [this](const std::string& endpoint_id,
                                                 NcDistanceInfo distance_info) {
    discovery_listener_.endpoint_distance_changed_cb(
        endpoint_id, static_cast<DistanceInfo>(distance_info));
  };

  GetService(service_handle_)
      ->StartDiscovery(service_id, options, std::move(listener),
                       BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::StopDiscovery(
    absl::string_view service_id, std::function<void(Status status)> callback) {
  GetService(service_handle_)->StopDiscovery(BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::RequestConnection(
    absl::string_view service_id, const std::vector<uint8_t>& endpoint_info,
    absl::string_view endpoint_id, ConnectionOptions connection_options,
    ConnectionListener connection_listener,
    std::function<void(Status status)> callback) {
  connection_listener_ = std::move(connection_listener);
  NcConnectionOptions options{};
  options.allowed.ble = connection_options.allowed_mediums.ble;
  options.allowed.bluetooth = connection_options.allowed_mediums.bluetooth;
  options.allowed.web_rtc = connection_options.allowed_mediums.web_rtc;
  options.allowed.wifi_lan = connection_options.allowed_mediums.wifi_lan;
  options.allowed.wifi_hotspot =
      connection_options.allowed_mediums.wifi_hotspot;
  if (connection_options.keep_alive_interval.has_value()) {
    options.keep_alive_interval_millis =
        *connection_options.keep_alive_interval / absl::Milliseconds(1);
  }
  if (connection_options.keep_alive_timeout.has_value()) {
    options.keep_alive_timeout_millis =
        *connection_options.keep_alive_timeout / absl::Milliseconds(1);
  }
  if (connection_options.remote_bluetooth_mac_address.has_value()) {
    auto mac_address = *connection_options.remote_bluetooth_mac_address;
    options.remote_bluetooth_mac_address =
        NcByteArray(std::string(mac_address.begin(), mac_address.end()));
  }
  options.non_disruptive_hotspot_mode =
      connection_options.non_disruptive_hotspot_mode;
  NcConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      NcByteArray(std::string(endpoint_info.begin(), endpoint_info.end()));
  connection_request_info.listener.initiated_cb =
      [&](const std::string& endpoint_id,
          const NcConnectionResponseInfo& info) {
        ConnectionInfo connection_info;
        connection_info.authentication_token = info.authentication_token;
        std::string remote_end_point = std::string(info.remote_endpoint_info);
        connection_info.endpoint_info = std::vector<uint8_t>(
            remote_end_point.begin(), remote_end_point.end());
        connection_info.is_incoming_connection = info.is_incoming_connection;
        std::string raw_authentication_token =
            std::string(info.raw_authentication_token);
        connection_info.raw_authentication_token = std::vector<uint8_t>(
            raw_authentication_token.begin(), raw_authentication_token.end());
        connection_listener_.initiated_cb(endpoint_id, connection_info);
      };
  connection_request_info.listener.accepted_cb =
      [&](const std::string& endpoint_id) {
        connection_listener_.accepted_cb(endpoint_id);
      };
  connection_request_info.listener.rejected_cb =
      [&](const std::string& endpoint_info, NcStatus status) {
        connection_listener_.rejected_cb(endpoint_info,
                                         ConvertToStatus(status));
      };
  connection_request_info.listener.disconnected_cb =
      [&](const std::string& endpoint_info) {
        connection_listener_.disconnected_cb(endpoint_info);
      };
  connection_request_info.listener.bandwidth_changed_cb =
      [&](const std::string& endpoint_id, NcMedium medium) {
        connection_listener_.bandwidth_changed_cb(endpoint_id,
                                                  static_cast<Medium>(medium));
      };

  GetService(service_handle_)
      ->RequestConnection(endpoint_id, std::move(connection_request_info),
                          options, BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::DisconnectFromEndpoint(
    absl::string_view service_id, absl::string_view endpoint_id,
    std::function<void(Status status)> callback) {
  GetService(service_handle_)
      ->DisconnectFromEndpoint(endpoint_id, BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::SendPayload(
    absl::string_view service_id, absl::Span<const std::string> endpoint_ids,
    std::unique_ptr<Payload> payload,
    std::function<void(Status status)> callback) {
  GetService(service_handle_)
      ->SendPayload(endpoint_ids, ConvertToServicePayload(*payload),
                    BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::CancelPayload(
    absl::string_view service_id, int64_t payload_id,
    std::function<void(Status status)> callback) {
  GetService(service_handle_)
      ->CancelPayload(payload_id, BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::InitiateBandwidthUpgrade(
    absl::string_view service_id, absl::string_view endpoint_id,
    std::function<void(Status status)> callback) {
  GetService(service_handle_)
      ->InitiateBandwidthUpgrade(endpoint_id, BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::AcceptConnection(
    absl::string_view service_id, absl::string_view endpoint_id,
    PayloadListener payload_listener,
    std::function<void(Status status)> callback) {
  payload_listeners_.emplace(endpoint_id, std::move(payload_listener));
  NcPayloadListener service_payload_listener{
      .payload_cb =
          [&](absl::string_view endpoint_id, NcPayload payload) {
            auto payload_listener = payload_listeners_.find(endpoint_id);
            if (payload_listener == payload_listeners_.end()) {
              return;
            }

            NEARBY_VLOG(1) << "payload callback id=" << payload.GetId();

            switch (payload.GetType()) {
              case NcPayloadType::kBytes:
              case NcPayloadType::kFile:
                payload_listener->second.payload_cb(
                    endpoint_id, ConvertToPayload(std::move(payload)));
                break;
              default:
                // TODO(b/219814719); support stream payload.
                break;
            }
          },
      .payload_progress_cb =
          [&](absl::string_view endpoint_id,
              const NcPayloadProgressInfo& info) {
            PayloadTransferUpdate transfer_update;
            transfer_update.bytes_transferred = info.bytes_transferred;
            transfer_update.payload_id = info.payload_id;
            transfer_update.status = static_cast<PayloadStatus>(info.status);
            transfer_update.total_bytes = info.total_bytes;
            NEARBY_VLOG(1) << "payload transfer update id=" << info.payload_id;
            auto payload_listener = payload_listeners_.find(endpoint_id);
            if (payload_listener != payload_listeners_.end()) {
              payload_listener->second.payload_progress_cb(endpoint_id,
                                                           transfer_update);
            }
          }};

  GetService(service_handle_)
      ->AcceptConnection(endpoint_id, std::move(service_payload_listener),
                         BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::StopAllEndpoints(
    std::function<void(Status status)> callback) {
  GetService(service_handle_)->StopAllEndpoints(BuildResultCallback(callback));
}

void NearbyConnectionsServiceImpl::SetCustomSavePath(
    absl::string_view path, std::function<void(Status status)> callback) {
  GetService(service_handle_)
      ->SetCustomSavePath(path, BuildResultCallback(callback));
}

std::string NearbyConnectionsServiceImpl::Dump() const {
  return GetService(service_handle_)->Dump();
}

}  // namespace sharing
}  // namespace nearby
