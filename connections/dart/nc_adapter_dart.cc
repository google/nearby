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

#include "connections/dart/nc_adapter_dart.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "third_party/dart_lang/v2/runtime/include/dart_api.h"
#include "third_party/dart_lang/v2/runtime/include/dart_api_dl.h"
#include "third_party/dart_lang/v2/runtime/include/dart_native_api.h"
#include "connections/c/nc.h"
#include "connections/c/nc_types.h"
#include "connections/dart/nc_adapter_types.h"
#include "connections/dart/nearby_connections_client_state.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/prng.h"

using nearby::connections::dart::NearbyConnectionsClientState;
using NearbyConnectionsApi = nearby::connections::dart::
    NearbyConnectionsClientState::NearbyConnectionsApi;

static absl::NoDestructor<NearbyConnectionsClientState> kClientState;

NC_STRATEGY_TYPE GetStrategy(StrategyDart strategy) {
  switch (strategy) {
    case STRATEGY_P2P_CLUSTER:
      return NC_STRATEGY_TYPE_P2P_CLUSTER;
    case STRATEGY_P2P_POINT_TO_POINT:
      return NC_STRATEGY_TYPE_P2P_POINT_TO_POINT;
    case STRATEGY_P2P_STAR:
      return NC_STRATEGY_TYPE_P2P_STAR;
    default:
      break;
  }
  return NC_STRATEGY_TYPE_NONE;
}

nearby::ByteArray ConvertBluetoothMacAddress(absl::string_view address) {
  return nearby::ByteArray(std::string(address));
}

NC_PAYLOAD_ID GeneratePayloadId() { return nearby::Prng().NextInt64(); }

void ResultCB(std::optional<Dart_Port> port, NC_STATUS status) {
  (void)status;  // Avoid unused parameter warning
  if (!port.has_value()) {
    NEARBY_LOGS(ERROR) << "ResultCB called with invalid port.";
    return;
  }

  Dart_CObject dart_object_result_callback;
  dart_object_result_callback.type = Dart_CObject_kInt64;
  dart_object_result_callback.value.as_int64 = static_cast<int64_t>(status);
  const bool result = Dart_PostCObject_DL(*port, &dart_object_result_callback);
  if (!result) {
    NEARBY_LOGS(WARNING) << "Posting message to port failed.";
  }
}

std::string GetEndpointIdString(int endpoint_id) {
  std::string endpoint_id_str;
  endpoint_id_str.resize(4);
  endpoint_id_str[0] = endpoint_id & 0xff;
  endpoint_id_str[1] = (endpoint_id >> 8) & 0xff;
  endpoint_id_str[2] = (endpoint_id >> 16) & 0xff;
  endpoint_id_str[3] = (endpoint_id >> 24) & 0xff;
  return endpoint_id_str;
}

void ListenerInitiatedCB(
    NC_INSTANCE instance, int endpoint_id,
    const NC_CONNECTION_RESPONSE_INFO *connection_response_info,
    void *context) {
  NEARBY_LOGS(INFO) << "Advertising initiated: id="
                    << GetEndpointIdString(endpoint_id);

  Dart_CObject dart_object_endpoint_id = {
      .type = Dart_CObject_Type::Dart_CObject_kInt32,
      .value = {.as_int32 = endpoint_id}};

  Dart_CObject dart_object_endpoint_info = {
      .type = Dart_CObject_Type::Dart_CObject_kTypedData,
      .value = {.as_typed_data{
          .type = Dart_TypedData_Type::Dart_TypedData_kUint8,
          .length =
              (intptr_t)connection_response_info->remote_endpoint_info.size,
          .values =
              (uint8_t *)connection_response_info->remote_endpoint_info.data}}};

  Dart_CObject *elements[2];
  elements[0] = &dart_object_endpoint_id;
  elements[1] = &dart_object_endpoint_info;

  Dart_CObject dart_object_initiated;
  dart_object_initiated.type = Dart_CObject_kArray;
  dart_object_initiated.value.as_array.length = 2;
  dart_object_initiated.value.as_array.values = elements;

  const bool result = Dart_PostCObject_DL(
      kClientState->GetConnectionListenerDart()->initiated_dart_port,
      &dart_object_initiated);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerAcceptedCB(NC_INSTANCE instance, int endpoint_id, void *context) {
  NEARBY_LOGS(INFO) << "Advertising accepted: id="
                    << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_accepted;
  dart_object_accepted.type = Dart_CObject_kInt32;
  dart_object_accepted.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetConnectionListenerDart()->accepted_dart_port,
      &dart_object_accepted);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerRejectedCB(NC_INSTANCE instance, int endpoint_id, NC_STATUS status,
                        void *context) {
  NEARBY_LOGS(INFO) << "Advertising rejected: id="
                    << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_rejected;
  dart_object_rejected.type = Dart_CObject_kInt32;
  dart_object_rejected.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetConnectionListenerDart()->rejected_dart_port,
      &dart_object_rejected);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerDisconnectedCB(NC_INSTANCE instance, int endpoint_id,
                            void *context) {
  NEARBY_LOGS(INFO) << "Advertising disconnected: id="
                    << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_disconnected;
  dart_object_disconnected.type = Dart_CObject_kInt32;
  dart_object_disconnected.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetConnectionListenerDart()->disconnected_dart_port,
      &dart_object_disconnected);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerBandwidthChangedCB(NC_INSTANCE instance, int endpoint_id,
                                NC_MEDIUM medium, void *context) {
  NEARBY_LOGS(INFO) << "Advertising bandwidth changed: id="
                    << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_bandwidth_changed;

  dart_object_bandwidth_changed.type = Dart_CObject_kInt32;
  dart_object_bandwidth_changed.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetConnectionListenerDart()->bandwidth_changed_dart_port,
      &dart_object_bandwidth_changed);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerEndpointFoundCB(NC_INSTANCE instance, int endpoint_id,
                             const NC_DATA *endpoint_info,
                             const NC_DATA *service_id, void *context) {
  NEARBY_LOGS(INFO) << "Device discovered: id="
                    << GetEndpointIdString(endpoint_id);
  NEARBY_LOGS(INFO) << "Device discovered: service_id="
                    << std::string(service_id->data, service_id->size);

  std::string endpoint_info_str = absl::BytesToHexString(
      absl::string_view(endpoint_info->data, endpoint_info->size));
  NEARBY_LOGS(INFO) << "Device discovered: info=" << endpoint_info_str;

  Dart_CObject dart_object_endpoint_id = {
      .type = Dart_CObject_Type::Dart_CObject_kInt32,
      .value = {.as_int32 = endpoint_id}};

  Dart_CObject dart_object_endpoint_info = {
      .type = Dart_CObject_Type::Dart_CObject_kTypedData,
      .value = {
          .as_typed_data{.type = Dart_TypedData_Type::Dart_TypedData_kUint8,
                         .length = (intptr_t)endpoint_info->size,
                         .values = (uint8_t *)endpoint_info->data}}};

  Dart_CObject *elements[2];
  elements[0] = &dart_object_endpoint_id;
  elements[1] = &dart_object_endpoint_info;

  Dart_CObject dart_object_found;
  dart_object_found.type = Dart_CObject_kArray;
  dart_object_found.value.as_array.length = 2;
  dart_object_found.value.as_array.values = elements;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetDiscoveryListenerDart()->found_dart_port,
      &dart_object_found);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerEndpointLostCB(NC_INSTANCE instance, int endpoint_id,
                            void *context) {
  NEARBY_LOGS(INFO) << "Device lost: id=" << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_lost;
  dart_object_lost.type = Dart_CObject_kInt32;
  dart_object_lost.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetDiscoveryListenerDart()->lost_dart_port,
      &dart_object_lost);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerEndpointDistanceChangedCB(NC_INSTANCE instance, int endpoint_id,
                                       NC_DISTANCE_INFO distance_info,
                                       void *context) {
  (void)distance_info;  // Avoid unused parameter warning
  NEARBY_LOGS(INFO) << "Device distance changed: id="
                    << GetEndpointIdString(endpoint_id);
  Dart_CObject dart_object_distance_changed;
  dart_object_distance_changed.type = Dart_CObject_kInt32;
  dart_object_distance_changed.value.as_int32 = endpoint_id;
  const bool result = Dart_PostCObject_DL(
      kClientState->GetDiscoveryListenerDart()->distance_changed_dart_port,
      &dart_object_distance_changed);
  if (!result) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void ListenerPayloadCB(NC_INSTANCE instance, int endpoint_id,
                       const NC_PAYLOAD *payload, void *context) {
  NEARBY_LOGS(INFO) << "Payload callback called. id: "
                    << GetEndpointIdString(endpoint_id)
                    << ", payload_id: " << payload->id
                    << ", type: " << payload->type;

  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kInt32;
  dart_object_endpoint_id.value.as_int32 = endpoint_id;

  Dart_CObject dart_object_payload_id;
  dart_object_payload_id.type = Dart_CObject_kInt64;
  dart_object_payload_id.value.as_int64 = payload->id;

  switch (payload->type) {
    case NC_PAYLOAD_TYPE_BYTES: {
      const char *bytes = payload->content.bytes.content.data;
      size_t bytes_size = payload->content.bytes.content.size;

      if (bytes_size == 0) {
        NEARBY_LOGS(INFO) << "Failed to get the payload as bytes.";
        return;
      }

      Dart_CObject dart_object_bytes;
      dart_object_bytes.type = Dart_CObject_kTypedData;
      dart_object_bytes.value.as_typed_data = {
          .type = Dart_TypedData_kUint8,
          .length = static_cast<intptr_t>(bytes_size),
          .values = reinterpret_cast<const uint8_t *>(bytes),
      };

      Dart_CObject *elements[] = {
          &dart_object_endpoint_id,
          &dart_object_payload_id,
          &dart_object_bytes,
      };

      Dart_CObject dart_object_payload;
      dart_object_payload.type = Dart_CObject_kArray;
      dart_object_payload.value.as_array.length = 3;
      dart_object_payload.value.as_array.values = elements;
      if (!Dart_PostCObject_DL(
              kClientState->GetPayloadListenerDart()->initial_byte_info_port,
              &dart_object_payload)) {
        NEARBY_LOGS(INFO) << "Posting message to port failed.";
      }
      return;
    }
    case NC_PAYLOAD_TYPE_STREAM: {
      Dart_CObject *elements[] = {
          &dart_object_endpoint_id,
          &dart_object_payload_id,
      };

      Dart_CObject dart_object_payload;
      dart_object_payload.type = Dart_CObject_kArray;
      dart_object_payload.value.as_array.length = 2;
      dart_object_payload.value.as_array.values = elements;
      if (!Dart_PostCObject_DL(
              kClientState->GetPayloadListenerDart()->initial_stream_info_port,
              &dart_object_payload)) {
        NEARBY_LOGS(INFO) << "Posting message to port failed.";
      }
      return;
    }
    case NC_PAYLOAD_TYPE_FILE: {
      Dart_CObject dart_object_offset;
      dart_object_offset.type = Dart_CObject_kInt64;
      dart_object_offset.value.as_int64 = payload->content.file.offset;

      std::string path = payload->content.file.file_name;
      Dart_CObject dart_object_path;
      dart_object_path.type = Dart_CObject_kString;
      dart_object_path.value.as_string = const_cast<char *>(path.c_str());

      Dart_CObject *elements[] = {
          &dart_object_endpoint_id,
          &dart_object_payload_id,
          &dart_object_offset,
          &dart_object_path,
      };

      Dart_CObject dart_object_payload;
      dart_object_payload.type = Dart_CObject_kArray;
      dart_object_payload.value.as_array.length = 4;
      dart_object_payload.value.as_array.values = elements;
      if (!Dart_PostCObject_DL(
              kClientState->GetPayloadListenerDart()->initial_file_info_port,
              &dart_object_payload)) {
        NEARBY_LOGS(INFO) << "Posting message to port failed.";
      }
      return;
    }
    default:
      NEARBY_LOGS(INFO) << "Invalid payload type.";
      return;
  }
}

void ListenerPayloadProgressCB(
    NC_INSTANCE instance, int endpoint_id,
    const NC_PAYLOAD_PROGRESS_INFO *payload_progress_info, void *context) {
  NEARBY_LOGS(INFO) << "Payload progress callback called. id: "
                    << GetEndpointIdString(endpoint_id)
                    << ", payload_id: " << payload_progress_info->id
                    << ", bytes transferred: "
                    << payload_progress_info->bytes_transferred
                    << ", total: " << payload_progress_info->total_bytes
                    << ", status: " << payload_progress_info->status;
  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kInt32;
  dart_object_endpoint_id.value.as_int32 = endpoint_id;

  Dart_CObject dart_object_payload_id;
  dart_object_payload_id.type = Dart_CObject_kInt64;
  dart_object_payload_id.value.as_int64 = payload_progress_info->id;

  Dart_CObject dart_object_bytes_transferred;
  dart_object_bytes_transferred.type = Dart_CObject_kInt64;
  dart_object_bytes_transferred.value.as_int64 =
      payload_progress_info->bytes_transferred;

  Dart_CObject dart_object_total_bytes;
  dart_object_total_bytes.type = Dart_CObject_kInt64;
  dart_object_total_bytes.value.as_int64 = payload_progress_info->total_bytes;

  Dart_CObject dart_object_status;
  dart_object_status.type = Dart_CObject_kInt64;
  dart_object_status.value.as_int64 = (int64_t)payload_progress_info->status;

  Dart_CObject *elements[5];
  elements[0] = &dart_object_endpoint_id;
  elements[1] = &dart_object_payload_id;
  elements[2] = &dart_object_bytes_transferred;
  elements[3] = &dart_object_total_bytes;
  elements[4] = &dart_object_status;

  Dart_CObject dart_object_payload_progress;
  dart_object_payload_progress.type = Dart_CObject_kArray;
  dart_object_payload_progress.value.as_array.length = 5;
  dart_object_payload_progress.value.as_array.values = elements;

  if (!Dart_PostCObject_DL(
          kClientState->GetPayloadListenerDart()->payload_progress_dart_port,
          &dart_object_payload_progress)) {
    NEARBY_LOGS(INFO) << "Posting message to port failed.";
  }
}

void PostResult(Dart_Port &result_cb, NC_STATUS value) {
  Dart_CObject dart_object_result_callback;
  dart_object_result_callback.type = Dart_CObject_kInt64;
  dart_object_result_callback.value.as_int64 = static_cast<int64_t>(value);
  const bool result =
      Dart_PostCObject_DL(result_cb, &dart_object_result_callback);
  if (!result) {
    NEARBY_LOGS(INFO) << "Returning error to port failed.";
  }
}

NC_INSTANCE CreateServiceDart() {
  NC_INSTANCE instance = kClientState->GetOpennedService();
  if (instance == nullptr) {
    instance = NcCreateService();
    kClientState->SetOpennedService(instance);
  }

  return instance;
}

void CloseServiceDart(NC_INSTANCE instance) {
  NcCloseService(instance);
  kClientState->reset();
}

int GetLocalEndpointIdDart(NC_INSTANCE instance) {
  return NcGetLocalEndpointId(instance);
}

void EnableBleV2Dart(NC_INSTANCE instance, int64_t enable,
                     Dart_Port result_cb) {
  kClientState->PushNearbyConnectionsApiPort(NearbyConnectionsApi::kEnableBleV2,
                                             result_cb);
  NcEnableBleV2(
      instance, enable,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kEnableBleV2),
                 status);
      },
      nullptr);
  NEARBY_LOGS(INFO) << "EnableBleV2Dart callback is called with enable="
                    << enable;
}

void StartAdvertisingDart(NC_INSTANCE instance, DataDart service_id,
                          AdvertisingOptionsDart options_dart,
                          ConnectionRequestInfoDart info_dart,
                          Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kStartAdvertising, result_cb);
  kClientState->SetConnectionListenerDart(
      std::make_unique<ConnectionListenerDart>(info_dart.connection_listener));

  NC_ADVERTISING_OPTIONS advertising_options{};
  advertising_options.common_options.strategy.type =
      GetStrategy(options_dart.strategy);
  advertising_options.auto_upgrade_bandwidth =
      options_dart.auto_upgrade_bandwidth;
  advertising_options.enforce_topology_constraints =
      options_dart.enforce_topology_constraints;

  advertising_options.low_power = options_dart.low_power;
  advertising_options.fast_advertisement_service_uuid.data =
      const_cast<char *>(options_dart.fast_advertisement_service_uuid.data);
  advertising_options.fast_advertisement_service_uuid.size =
      options_dart.fast_advertisement_service_uuid.size;

  advertising_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH] =
      options_dart.mediums.bluetooth != 0;
  advertising_options.common_options.allowed_mediums[NC_MEDIUM_BLE] =
      options_dart.mediums.ble != 0;
  advertising_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN] =
      options_dart.mediums.wifi_lan != 0;
  advertising_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT] =
      options_dart.mediums.wifi_hotspot;
  advertising_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC] =
      options_dart.mediums.web_rtc;

  NC_CONNECTION_REQUEST_INFO request_info{};

  request_info.endpoint_info.data = info_dart.endpoint_info.data;
  request_info.endpoint_info.size = info_dart.endpoint_info.size;
  request_info.initiated_callback = &ListenerInitiatedCB;
  request_info.accepted_callback = ListenerAcceptedCB;
  request_info.rejected_callback = ListenerRejectedCB;
  request_info.disconnected_callback = ListenerDisconnectedCB;
  request_info.bandwidth_changed_callback = ListenerBandwidthChangedCB;

  NC_DATA service_id_data =
      NC_DATA{.size = service_id.size, .data = service_id.data};
  NcStartAdvertising(
      instance, &service_id_data, &advertising_options, &request_info,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kStartAdvertising),
                 status);
      },
      nullptr);
}

void StopAdvertisingDart(NC_INSTANCE instance, Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kStopAdvertising, result_cb);

  NcStopAdvertising(
      instance,
      [](NC_STATUS status, void *context) {
        kClientState->SetConnectionListenerDart(nullptr);
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kStopAdvertising),
                 status);
      },
      nullptr);
}

void StartDiscoveryDart(NC_INSTANCE instance, DataDart service_id,
                        DiscoveryOptionsDart options_dart,
                        DiscoveryListenerDart listener_dart,
                        Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kStartDiscovery, result_cb);
  kClientState->SetDiscoveryListenerDart(
      std::make_unique<DiscoveryListenerDart>(listener_dart));

  NC_DISCOVERY_OPTIONS discovery_options{};
  discovery_options.common_options.strategy.type =
      GetStrategy(options_dart.strategy);
  discovery_options.enforce_topology_constraints = true;
  // This needs to be passed in by the UI. If it's null, then no
  // fast_advertisement_service. Otherwise this interface will always
  // and forever be locked into 0000FE2C-0000-1000-8000-00805F9B34FB
  // whenever fast advertisement service is requested.
  discovery_options.fast_advertisement_service_uuid.data =
      const_cast<char *>(options_dart.fast_advertisement_service_uuid.data);
  discovery_options.fast_advertisement_service_uuid.size =
      options_dart.fast_advertisement_service_uuid.size;

  discovery_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH] =
      options_dart.mediums.bluetooth != 0;
  discovery_options.common_options.allowed_mediums[NC_MEDIUM_BLE] =
      options_dart.mediums.ble != 0;
  discovery_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN] =
      options_dart.mediums.wifi_lan != 0;
  discovery_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT] =
      options_dart.mediums.wifi_hotspot;
  discovery_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC] =
      options_dart.mediums.web_rtc;
  discovery_options.low_power = options_dart.low_power;

  NC_DISCOVERY_LISTENER listener{};
  listener.endpoint_distance_changed_callback =
      ListenerEndpointDistanceChangedCB;
  listener.endpoint_found_callback = &ListenerEndpointFoundCB;
  listener.endpoint_lost_callback = &ListenerEndpointLostCB;

  NC_DATA service_id_data =
      NC_DATA{.size = service_id.size, .data = service_id.data};
  NcStartDiscovery(
      instance, &service_id_data, &discovery_options, &listener,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kStartDiscovery),
                 status);
      },
      nullptr);
}

void StopDiscoveryDart(NC_INSTANCE instance, Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kStopDiscovery, result_cb);

  NcStopDiscovery(
      instance,
      [](NC_STATUS status, void *context) {
        kClientState->SetDiscoveryListenerDart(nullptr);
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kStopDiscovery),
                 status);
      },
      nullptr);
}

void RequestConnectionDart(NC_INSTANCE instance, int endpoint_id,
                           ConnectionOptionsDart options_dart,
                           ConnectionRequestInfoDart info_dart,
                           Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kRequestConnection, result_cb);
  kClientState->SetConnectionListenerDart(
      std::make_unique<ConnectionListenerDart>(info_dart.connection_listener));

  NC_CONNECTION_OPTIONS connection_options;
  connection_options.enforce_topology_constraints =
      options_dart.enforce_topology_constraints;
  connection_options.remote_bluetooth_mac_address.data =
      options_dart.remote_bluetooth_mac_address.data;
  connection_options.remote_bluetooth_mac_address.size =
      options_dart.remote_bluetooth_mac_address.size;
  connection_options.fast_advertisement_service_uuid.data =
      options_dart.fast_advertisement_service_uuid.data;
  connection_options.fast_advertisement_service_uuid.size =
      options_dart.fast_advertisement_service_uuid.size;
  connection_options.keep_alive_interval_millis =
      options_dart.keep_alive_interval_millis;
  connection_options.keep_alive_timeout_millis =
      options_dart.keep_alive_timeout_millis;

  connection_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH] =
      options_dart.mediums.bluetooth != 0;
  connection_options.common_options.allowed_mediums[NC_MEDIUM_BLE] =
      options_dart.mediums.ble != 0;
  connection_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN] =
      options_dart.mediums.wifi_lan != 0;
  connection_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT] =
      options_dart.mediums.wifi_hotspot;
  connection_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC] =
      options_dart.mediums.web_rtc;

  NC_CONNECTION_REQUEST_INFO request_info{};

  request_info.endpoint_info.data = info_dart.endpoint_info.data;
  request_info.endpoint_info.size = info_dart.endpoint_info.size;
  request_info.initiated_callback = ListenerInitiatedCB;
  request_info.accepted_callback = ListenerAcceptedCB;
  request_info.rejected_callback = ListenerRejectedCB;
  request_info.disconnected_callback = ListenerDisconnectedCB;
  request_info.bandwidth_changed_callback = ListenerBandwidthChangedCB;

  NcRequestConnection(
      instance, endpoint_id, &request_info, &connection_options,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kRequestConnection),
                 status);
      },
      nullptr);
}

void AcceptConnectionDart(NC_INSTANCE instance, int endpoint_id,
                          PayloadListenerDart listener_dart,
                          Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kAcceptConnection, result_cb);
  kClientState->SetPayloadListenerDart(
      std::make_unique<PayloadListenerDart>(listener_dart));

  NC_PAYLOAD_LISTENER listener{};
  listener.received_callback = &ListenerPayloadCB;
  listener.progress_updated_callback = &ListenerPayloadProgressCB;

  NcAcceptConnection(
      instance, endpoint_id, listener,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kAcceptConnection),
                 status);
      },
      nullptr);
}

void RejectConnectionDart(NC_INSTANCE instance, int endpoint_id,
                          Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kRejectConnection, result_cb);

  NcRejectConnection(
      instance, endpoint_id,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kRejectConnection),
                 status);
      },
      nullptr);
}

void DisconnectFromEndpointDart(NC_INSTANCE instance, int endpoint_id,
                                Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(
      NearbyConnectionsApi::kDisconnectFromEndpoint, result_cb);

  NcDisconnectFromEndpoint(
      instance, endpoint_id,
      [](NC_STATUS status, void *context) {
        ResultCB(kClientState->PopNearbyConnectionsApiPort(
                     NearbyConnectionsApi::kDisconnectFromEndpoint),
                 status);
      },
      nullptr);
}

void SendPayloadDart(NC_INSTANCE instance, int endpoint_id,
                     PayloadDart payload_dart, Dart_Port result_cb) {
  if (instance == nullptr) {
    PostResult(result_cb, NC_STATUS_ERROR);
    return;
  }

  kClientState->PushNearbyConnectionsApiPort(NearbyConnectionsApi::kSendPayload,
                                             result_cb);
  std::vector<int> endpoint_ids = {endpoint_id};

  NEARBY_LOGS(INFO) << "Payload type: " << payload_dart.type;
  switch (payload_dart.type) {
    case PAYLOAD_TYPE_UNKNOWN:
    case PAYLOAD_TYPE_STREAM:
      NEARBY_LOGS(INFO) << "Payload type not supported yet";
      PostResult(result_cb, NC_STATUS_PAYLOADUNKNOWN);
      break;
    case PAYLOAD_TYPE_BYTE: {
      NC_PAYLOAD payload{};
      payload.id = GeneratePayloadId();
      payload.type = NC_PAYLOAD_TYPE_BYTES;
      payload.direction = NC_PAYLOAD_DIRECTION_INCOMING;
      payload.content.bytes.content.data = payload_dart.data.data;
      payload.content.bytes.content.size = payload_dart.data.size;

      const int *endpoint_ids_ptr = endpoint_ids.data();
      NcSendPayload(
          instance, endpoint_ids.size(), endpoint_ids_ptr, &payload,
          [](NC_STATUS status, void *context) {
            ResultCB(kClientState->PopNearbyConnectionsApiPort(
                         NearbyConnectionsApi::kSendPayload),
                     status);
          },
          nullptr);
      break;
    }
    case PAYLOAD_TYPE_FILE:
      NEARBY_LOGS(INFO) << "File name: "
                        << std::string(payload_dart.data.data,
                                       payload_dart.data.size)
                        << ", size " << payload_dart.size;
      std::string file_name_str(payload_dart.data.data, payload_dart.data.size);

      NC_PAYLOAD payload{};
      payload.id = GeneratePayloadId();
      payload.type = NC_PAYLOAD_TYPE_FILE;
      payload.direction = NC_PAYLOAD_DIRECTION_OUTGOING;
      payload.content.file.file_name =
          const_cast<char *>(file_name_str.c_str());
      payload.content.file.parent_folder = nullptr;
      const int *endpoint_ids_ptr = endpoint_ids.data();
      NC_PAYLOAD moved_payload = std::move(payload);
      NcSendPayload(
          instance, endpoint_ids.size(), endpoint_ids_ptr, &moved_payload,
          [](NC_STATUS status, void *context) {
            ResultCB(kClientState->PopNearbyConnectionsApiPort(
                         NearbyConnectionsApi::kSendPayload),
                     status);
          },
          nullptr);

      break;
  }
}
