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
#include <string>
#include <utility>
#include <vector>

#include "location/nearby/analytics/cpp/logging/event_logger.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/core.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/analytics_recorder_impl.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/payload_type.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::analytics::AnalyticsRecorderImpl;
using ::nearby::connections::ConnectionRequestInfo;
using ::nearby::connections::ConnectionResponseInfo;
using ::nearby::connections::Core;
using ::nearby::connections::PayloadProgressInfo;
using ::nearby::connections::PayloadType;
using ::nearby::connections::ServiceControllerRouter;

using NcAdvertisingOptions = ::nearby::connections::AdvertisingOptions;
using NcConnectionListener = ::nearby::connections::ConnectionListener;
using NcConnectionOptions = ::nearby::connections::ConnectionOptions;
using NcDiscoveryListener = ::nearby::connections::DiscoveryListener;
using NcDiscoveryOptions = ::nearby::connections::DiscoveryOptions;
using NcDistanceInfo = ::nearby::connections::DistanceInfo;
using NcMedium = ::nearby::connections::Medium;
using NcPayload = ::nearby::connections::Payload;
using NcPayloadListener = ::nearby::connections::PayloadListener;
using NcStatus = ::nearby::connections::Status;

Core* GetService(NearbyConnectionsServiceImpl::HANDLE handle) {
  return reinterpret_cast<Core*>(handle);
}

// Adapts a sharing `NearbyConnectionsService::ConnectionListener` to a
// `nearby::connections::ConnectionListener`, keeping the listener alive via
// `std::shared_ptr` across asynchronous Core callbacks.
NcConnectionListener ToNcConnectionListener(
    NearbyConnectionsService::ConnectionListener listener) {
  auto shared_listener =
      std::make_shared<NearbyConnectionsService::ConnectionListener>(
          std::move(listener));
  NcConnectionListener nc_listener;
  nc_listener.initiated_cb =
      [shared_listener](const std::string& endpoint_id,
                        const ConnectionResponseInfo& info) {
        ConnectionInfo connection_info;
        connection_info.authentication_token = info.authentication_token;
        connection_info.endpoint_info = std::vector<uint8_t>(
            info.remote_endpoint_info.begin(), info.remote_endpoint_info.end());
        connection_info.is_incoming_connection = info.is_incoming_connection;
        connection_info.raw_authentication_token = std::vector<uint8_t>(
            info.raw_authentication_token.begin(),
            info.raw_authentication_token.end());
        shared_listener->initiated_cb(endpoint_id, connection_info);
      };
  nc_listener.accepted_cb = [shared_listener](const std::string& endpoint_id) {
    shared_listener->accepted_cb(endpoint_id);
  };
  nc_listener.rejected_cb = [shared_listener](const std::string& endpoint_id,
                                              NcStatus status) {
    shared_listener->rejected_cb(endpoint_id, ConvertToStatus(status));
  };
  nc_listener.disconnected_cb =
      [shared_listener](const std::string& endpoint_id) {
        shared_listener->disconnected_cb(endpoint_id);
      };
  nc_listener.bandwidth_changed_cb =
      [shared_listener = std::move(shared_listener)](
          const std::string& endpoint_id, NcMedium medium) {
        shared_listener->bandwidth_changed_cb(endpoint_id,
                                              static_cast<Medium>(medium));
      };
  return nc_listener;
}

}  // namespace

NearbyConnectionsServiceImpl::NearbyConnectionsServiceImpl(
    nearby::ConnectivityManager* connectivity_manager,
    nearby::analytics::EventLogger* event_logger)
    : connectivity_manager_(*connectivity_manager) {
  static ServiceControllerRouter* router =
      new ServiceControllerRouter([this]() {
        // WARNING: there can be only 1 instance of
        // NearbyConnectionsServiceImpl, otherwise the router could be pointing
        // at an invalid instance.
        return connectivity_manager_.IsHPRealtekDevice();
      });
  static Core* core =
      new Core(std::make_unique<AnalyticsRecorderImpl>(event_logger), router);
  service_handle_ = core;
}

NearbyConnectionsServiceImpl::~NearbyConnectionsServiceImpl() = default;

void NearbyConnectionsServiceImpl::StartAdvertising(
    absl::string_view service_id, const std::vector<uint8_t>& endpoint_info,
    const AdvertisingOptions& advertising_options,
    ConnectionListener advertising_listener,
    std::function<void(Status status)> callback) {
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
  options.force_new_endpoint_id = advertising_options.force_new_endpoint_id;
  options.fast_advertisement_service_uuid =
      advertising_options.fast_advertisement_service_uuid.uuid;

  ConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      ByteArray(std::string(endpoint_info.begin(), endpoint_info.end()));
  connection_request_info.listener =
      ToNcConnectionListener(std::move(advertising_listener));

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
    absl::string_view service_id, const DiscoveryOptions& discovery_options,
    DiscoveryListener discovery_listener,
    std::function<void(Status status)> callback) {
  auto shared_listener =
      std::make_shared<DiscoveryListener>(std::move(discovery_listener));

  NcDiscoveryOptions options{};
  options.strategy = ConvertToServiceStrategy(discovery_options.strategy);
  // NcDiscoveryOptions enabled all mediums by default, we should apply the
  // settings from discovery_options.
  options.allowed.SetAll(false);
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

  if (discovery_options.alternate_service_uuid.has_value()) {
    options.ble_options.alternate_uuid =
        discovery_options.alternate_service_uuid;
  }

  NcDiscoveryListener listener;
  listener.endpoint_found_cb = [shared_listener](
                                   const std::string& endpoint_id,
                                   const ByteArray& endpoint_info,
                                   const std::string& service_id) {
    std::string endpoint_info_data = std::string(endpoint_info);
    shared_listener->endpoint_found_cb(
        endpoint_id,
        DiscoveredEndpointInfo(std::vector<uint8_t>(endpoint_info_data.begin(),
                                                    endpoint_info_data.end()),
                               service_id));
  };
  listener.endpoint_lost_cb =
      [shared_listener](const std::string& endpoint_id) {
        shared_listener->endpoint_lost_cb(endpoint_id);
      };
  listener.endpoint_distance_changed_cb =
      [shared_listener = std::move(shared_listener)](
          const std::string& endpoint_id, NcDistanceInfo distance_info) {
        shared_listener->endpoint_distance_changed_cb(
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
    absl::string_view endpoint_id, const ConnectionOptions& connection_options,
    ConnectionListener connection_listener,
    std::function<void(Status status)> callback) {
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
    MacAddress mac_address;
    MacAddress::FromBytes(
        absl::MakeConstSpan(*connection_options.remote_bluetooth_mac_address),
        mac_address);
    options.remote_bluetooth_mac_address = mac_address;
  }
  options.non_disruptive_hotspot_mode =
      connection_options.non_disruptive_hotspot_mode;
  ConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      ByteArray(std::string(endpoint_info.begin(), endpoint_info.end()));
  connection_request_info.listener =
      ToNcConnectionListener(std::move(connection_listener));

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
  auto shared_listener =
      std::make_shared<PayloadListener>(std::move(payload_listener));
  NcPayloadListener service_payload_listener{
      .payload_cb =
          [shared_listener](absl::string_view endpoint_id, NcPayload payload) {
            VLOG(1) << "payload callback id=" << payload.GetId();

            switch (payload.GetType()) {
              case PayloadType::kBytes:
              case PayloadType::kFile:
                shared_listener->payload_cb(
                    endpoint_id, ConvertToPayload(std::move(payload)));
                break;
              default:
                // TODO(b/219814719); support stream payload.
                break;
            }
          },
      .payload_progress_cb =
          [shared_listener = std::move(shared_listener)](
              absl::string_view endpoint_id, const PayloadProgressInfo& info) {
            PayloadTransferUpdate transfer_update;
            transfer_update.bytes_transferred = info.bytes_transferred;
            transfer_update.payload_id = info.payload_id;
            transfer_update.status = static_cast<PayloadStatus>(info.status);
            transfer_update.total_bytes = info.total_bytes;
            VLOG(1) << "payload transfer update id=" << info.payload_id;
            shared_listener->payload_progress_cb(endpoint_id, transfer_update);
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

void NearbyConnectionsServiceImpl::OverrideSavePath(
    absl::string_view endpoint_id, absl::string_view path) {
  GetService(service_handle_)->OverrideSavePath(endpoint_id, path);
}

std::string NearbyConnectionsServiceImpl::Dump() const {
  return GetService(service_handle_)->Dump();
}

}  // namespace sharing
}  // namespace nearby
