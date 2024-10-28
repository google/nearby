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

#include "connections/c/nc.h"

#include <stddef.h>
#include <sys/stat.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/c/nc_types.h"
#include "connections/connection_options.h"
#include "connections/core.h"
#include "connections/discovery_options.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/file.h"
#include "internal/platform/logging.h"

namespace nearby::connections {
class Core;
class ServiceController;
class ServiceControllerRouter;
class OfflineServiceController;
}  // namespace nearby::connections

typedef struct NcContext {
  ::nearby::connections::ServiceControllerRouter* router = nullptr;
  ::nearby::connections::Core* core = nullptr;
} NcContext;

absl::NoDestructor<absl::flat_hash_map<NC_INSTANCE, NcContext>> kNcContextMap;

int64_t getFileSize(const char* filename) {
  struct stat file_status;
  if (stat(filename, &file_status) < 0) {
    return -1;
  }

  return file_status.st_size;
}

int convertStringToInt(absl::string_view data) {
  if (data.size() != 4) {
    return 0;
  }

  return (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

std::string convertIntToString(int input) {
  std::string result;
  result.resize(4);
  result[0] = input & 0xff;
  result[1] = (input >> 8) & 0xff;
  result[2] = (input >> 16) & 0xff;
  result[3] = (input >> 24) & 0xff;
  return result;
}

NcContext* GetContext(NC_INSTANCE instance) {
  auto it = kNcContextMap->find(instance);
  if (it == kNcContextMap->end()) {
    return nullptr;
  }
  return &it->second;
}

::nearby::connections::ConnectionRequestInfo GetCppConnectionRequestInfo(
    NC_INSTANCE instance,
    const NC_CONNECTION_REQUEST_INFO& connection_request_info,
    CALLER_CONTEXT context) {
  ::nearby::connections::ConnectionRequestInfo cpp_connection_request_info;
  cpp_connection_request_info.endpoint_info =
      nearby::ByteArray(connection_request_info.endpoint_info.data,
                        connection_request_info.endpoint_info.size);
  ::nearby::connections::ConnectionListener cpp_connection_listener;
  cpp_connection_listener.accepted_cb = [=](const std::string& endpoint_id) {
    connection_request_info.accepted_callback(
        instance, convertStringToInt(endpoint_id), context);
  };
  cpp_connection_listener.bandwidth_changed_cb =
      [=](const std::string& endpoint_id,
          ::nearby::connections::Medium medium) {
        connection_request_info.bandwidth_changed_callback(
            instance, convertStringToInt(endpoint_id),
            static_cast<NC_MEDIUM>(medium), context);
      };
  cpp_connection_listener.disconnected_cb =
      [=](const std::string& endpoint_id) {
        connection_request_info.disconnected_callback(
            instance, convertStringToInt(endpoint_id), context);
      };
  cpp_connection_listener.initiated_cb =
      [=](const std::string& endpoint_id,
          const ::nearby::connections::ConnectionResponseInfo& info) {
        NC_CONNECTION_RESPONSE_INFO connection_response_info;
        connection_response_info.is_connection_verified =
            info.is_connection_verified;
        connection_response_info.is_incoming_connection =
            info.is_incoming_connection;
        connection_response_info.remote_endpoint_info.data =
            (char*)info.remote_endpoint_info.data();
        connection_response_info.remote_endpoint_info.size =
            info.remote_endpoint_info.size();
        connection_response_info.authentication_token.data =
            (char*)info.authentication_token.data();
        connection_response_info.authentication_token.size =
            info.authentication_token.size();
        connection_response_info.raw_authentication_token.data =
            (char*)info.raw_authentication_token.data();
        connection_response_info.raw_authentication_token.size =
            info.raw_authentication_token.size();

        connection_request_info.initiated_callback(
            instance, convertStringToInt(endpoint_id),
            &connection_response_info, context);
      };

  cpp_connection_listener.rejected_cb =
      [=](const std::string& endpoint_id,
          ::nearby::connections::Status status) {
        connection_request_info.rejected_callback(
            instance, convertStringToInt(endpoint_id),
            static_cast<NC_STATUS>(status.value), context);
      };

  cpp_connection_request_info.listener = std::move(cpp_connection_listener);
  return cpp_connection_request_info;
}

NC_INSTANCE NcCreateService() {
  NcContext nc_context;
  nc_context.router = new ::nearby::connections::ServiceControllerRouter();
  nc_context.core = new ::nearby::connections::Core(nc_context.router);

  kNcContextMap->insert({nc_context.core, nc_context});
  return nc_context.core;
}

void NcCloseService(NC_INSTANCE instance) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    NEARBY_LOGS(WARNING) << "Trying to close not existent service " << instance;
    return;
  }

  nc_context->core->StopAllEndpoints([](::nearby::connections::Status status) {
    NEARBY_LOGS(INFO) << "Stopping all endpoints with status "
                      << status.ToString();
  });

  kNcContextMap->erase(nc_context->core);
  delete nc_context->router;
  delete nc_context->core;
}

void NcStartAdvertising(
    NC_INSTANCE instance, const NC_DATA* service_id,
    const NC_ADVERTISING_OPTIONS* advertising_options,
    const NC_CONNECTION_REQUEST_INFO* connection_request_info,
    NcCallbackResult result_callback, CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  ::nearby::connections::ConnectionRequestInfo cpp_connection_request_info =
      GetCppConnectionRequestInfo(instance, *connection_request_info, context);

  ::nearby::connections::AdvertisingOptions cpp_advertising_options;
  cpp_advertising_options.allowed.ble =
      advertising_options->common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_advertising_options.allowed.bluetooth =
      advertising_options->common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_advertising_options.allowed.wifi_lan =
      advertising_options->common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_advertising_options.allowed.wifi_direct =
      advertising_options->common_options
          .allowed_mediums[NC_MEDIUM_WIFI_DIRECT];
  cpp_advertising_options.allowed.wifi_hotspot =
      advertising_options->common_options
          .allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT];
  cpp_advertising_options.allowed.web_rtc =
      advertising_options->common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];
  cpp_advertising_options.enable_bluetooth_listening =
      advertising_options->enable_bluetooth_listening;
  cpp_advertising_options.enable_webrtc_listening =
      advertising_options->enable_webrtc_listening;
  cpp_advertising_options.auto_upgrade_bandwidth =
      advertising_options->auto_upgrade_bandwidth;
  cpp_advertising_options.enforce_topology_constraints =
      advertising_options->enforce_topology_constraints;
  if (advertising_options->fast_advertisement_service_uuid.size > 0) {
    cpp_advertising_options.fast_advertisement_service_uuid =
        std::string(advertising_options->fast_advertisement_service_uuid.data,
                    advertising_options->fast_advertisement_service_uuid.size);
  }

  cpp_advertising_options.is_out_of_band_connection =
      advertising_options->is_out_of_band_connection;
  cpp_advertising_options.low_power = advertising_options->low_power;

  if (advertising_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_NONE) {
    cpp_advertising_options.strategy = ::nearby::connections::Strategy::kNone;
  }
  if (advertising_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_advertising_options.strategy =
        ::nearby::connections::Strategy::kP2pCluster;
  if (advertising_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_advertising_options.strategy =
        ::nearby::connections::Strategy::kP2pPointToPoint;
  if (advertising_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_advertising_options.strategy =
        ::nearby::connections::Strategy::kP2pStar;

  nc_context->core->StartAdvertising(
      std::string(service_id->data, service_id->size),
      std::move(cpp_advertising_options),
      std::move(cpp_connection_request_info),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcStopAdvertising(NC_INSTANCE instance, NcCallbackResult result_callback,
                       CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->StopAdvertising([=](::nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value), context);
  });
}

void NcStartDiscovery(NC_INSTANCE instance, const NC_DATA* service_id,
                      const NC_DISCOVERY_OPTIONS* discovery_options,
                      const NC_DISCOVERY_LISTENER* discovery_listener,
                      NcCallbackResult result_callback, void* context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  ::nearby::connections::DiscoveryOptions cpp_discovery_options;

  if (discovery_options->common_options.strategy.type == NC_STRATEGY_TYPE_NONE)
    cpp_discovery_options.strategy = ::nearby::connections::Strategy::kNone;
  if (discovery_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_discovery_options.strategy =
        ::nearby::connections::Strategy::kP2pCluster;
  if (discovery_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_discovery_options.strategy =
        ::nearby::connections::Strategy::kP2pPointToPoint;
  if (discovery_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_discovery_options.strategy = ::nearby::connections::Strategy::kP2pStar;
  cpp_discovery_options.auto_upgrade_bandwidth =
      discovery_options->auto_upgrade_bandwidth;
  cpp_discovery_options.enforce_topology_constraints =
      discovery_options->enforce_topology_constraints;
  cpp_discovery_options.is_out_of_band_connection =
      discovery_options->is_out_of_band_connection;
  if (discovery_options->fast_advertisement_service_uuid.size > 0) {
    cpp_discovery_options.fast_advertisement_service_uuid =
        std::string(discovery_options->fast_advertisement_service_uuid.data,
                    discovery_options->fast_advertisement_service_uuid.size);
  }
  cpp_discovery_options.low_power = discovery_options->low_power;
  cpp_discovery_options.allowed.bluetooth =
      discovery_options->common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_discovery_options.allowed.ble =
      discovery_options->common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_discovery_options.allowed.wifi_lan =
      discovery_options->common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_discovery_options.allowed.wifi_hotspot =
      discovery_options->common_options.allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT];
  cpp_discovery_options.allowed.web_rtc =
      discovery_options->common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];

  NC_DISCOVERY_LISTENER discovery_listener_copy = *discovery_listener;
  ::nearby::connections::DiscoveryListener listener;
  listener.endpoint_distance_changed_cb =
      [=](const std::string& endpoint_id,
          ::nearby::connections::DistanceInfo info) {
        discovery_listener_copy.endpoint_distance_changed_callback(
            instance, convertStringToInt(endpoint_id),
            static_cast<NC_DISTANCE_INFO>(info), context);
      };
  listener.endpoint_found_cb = [=](const std::string& endpoint_id,
                                   const nearby::ByteArray& endpoint_info,
                                   const std::string& service_id) {
    NC_DATA endpoint_info_data = {
        .size = static_cast<int64_t>(endpoint_info.size()),
        .data = (char*)endpoint_info.data()};
    NC_DATA service_id_data = {.size = static_cast<int64_t>(service_id.size()),
                               .data = (char*)service_id.data()};
    discovery_listener_copy.endpoint_found_callback(
        instance, convertStringToInt(endpoint_id), &endpoint_info_data,
        &service_id_data, context);
  };

  listener.endpoint_lost_cb = [=](const std::string& endpoint_id) {
    discovery_listener_copy.endpoint_lost_callback(
        instance, convertStringToInt(endpoint_id), context);
  };
  nc_context->core->StartDiscovery(
      std::string(service_id->data, service_id->size),
      std::move(cpp_discovery_options), std::move(listener),
      [result_callback = std::move(result_callback),
       context](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcStopDiscovery(NC_INSTANCE instance, NcCallbackResult result_callback,
                     CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->StopDiscovery([=](::nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value), context);
  });
}

void NcInjectEndpoint(NC_INSTANCE instance, const NC_DATA* service_id,
                      const NC_OUT_OF_BAND_CONNECTION_METADATA* metadata,
                      NcCallbackResult result_callback,
                      CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  ::nearby::connections::OutOfBandConnectionMetadata
      cpp_out_of_band_connection_metadata;
  cpp_out_of_band_connection_metadata.endpoint_id = metadata->endpoint_id;
  cpp_out_of_band_connection_metadata.endpoint_info = {
      metadata->endpoint_info.data,
      static_cast<size_t>(metadata->endpoint_info.size)};
  cpp_out_of_band_connection_metadata.medium =
      static_cast<::nearby::connections::Medium>(metadata->medium);
  cpp_out_of_band_connection_metadata.remote_bluetooth_mac_address = {
      metadata->remote_bluetooth_mac_address.data,
      static_cast<size_t>(metadata->remote_bluetooth_mac_address.size)};

  nc_context->core->InjectEndpoint(
      std::string(service_id->data, service_id->size),
      cpp_out_of_band_connection_metadata,
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcRequestConnection(
    NC_INSTANCE instance, int endpoint_id,
    const NC_CONNECTION_REQUEST_INFO* connection_request_info,
    const NC_CONNECTION_OPTIONS* connection_options,
    NcCallbackResult result_callback, CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  ::nearby::connections::ConnectionRequestInfo cpp_connection_request_info =
      GetCppConnectionRequestInfo(instance, *connection_request_info, context);

  ::nearby::connections::ConnectionOptions cpp_connection_options;
  cpp_connection_options.allowed.ble =
      connection_options->common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_connection_options.allowed.bluetooth =
      connection_options->common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_connection_options.allowed.web_rtc =
      connection_options->common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];
  cpp_connection_options.allowed.wifi_lan =
      connection_options->common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_connection_options.auto_upgrade_bandwidth =
      connection_options->auto_upgrade_bandwidth;
  cpp_connection_options.enforce_topology_constraints =
      connection_options->enforce_topology_constraints;
  if (connection_options->fast_advertisement_service_uuid.size > 0) {
    cpp_connection_options.fast_advertisement_service_uuid =
        std::string(connection_options->fast_advertisement_service_uuid.data,
                    connection_options->fast_advertisement_service_uuid.size);
  }
  cpp_connection_options.is_out_of_band_connection =
      connection_options->is_out_of_band_connection;
  cpp_connection_options.keep_alive_interval_millis =
      connection_options->keep_alive_interval_millis;
  cpp_connection_options.keep_alive_timeout_millis =
      connection_options->keep_alive_timeout_millis;
  cpp_connection_options.low_power = connection_options->low_power;
  if (connection_options->remote_bluetooth_mac_address.size > 0) {
    cpp_connection_options.remote_bluetooth_mac_address =
        nearby::BluetoothUtils::FromString(
            std::string(connection_options->remote_bluetooth_mac_address.data,
                        connection_options->remote_bluetooth_mac_address.size));
  }
  if (connection_options->common_options.strategy.type == NC_STRATEGY_TYPE_NONE)
    cpp_connection_options.strategy = ::nearby::connections::Strategy::kNone;
  if (connection_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_connection_options.strategy =
        ::nearby::connections::Strategy::kP2pCluster;
  if (connection_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_connection_options.strategy =
        ::nearby::connections::Strategy::kP2pPointToPoint;
  if (connection_options->common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_connection_options.strategy = ::nearby::connections::Strategy::kP2pStar;

  nc_context->core->RequestConnection(
      convertIntToString(endpoint_id), std::move(cpp_connection_request_info),
      std::move(cpp_connection_options),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcAcceptConnection(NC_INSTANCE instance, int endpoint_id,
                        NC_PAYLOAD_LISTENER payload_listener,
                        NcCallbackResult result_callback,
                        CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  ::nearby::connections::PayloadListener cpp_payload_listener;
  cpp_payload_listener.payload_cb =
      [=](absl::string_view endpoint_id,
          ::nearby::connections::Payload payload) {
        NC_PAYLOAD nc_payload;
        nc_payload.id = payload.GetId();
        nc_payload.direction = NC_PAYLOAD_DIRECTION_INCOMING;
        nc_payload.type = static_cast<NC_PAYLOAD_TYPE>(payload.GetType());
        if (nc_payload.type == NC_PAYLOAD_TYPE_BYTES) {
          const nearby::ByteArray& bytes = payload.AsBytes();
          nc_payload.content.bytes.content.data =
              const_cast<char*>(bytes.data());
          nc_payload.content.bytes.content.size = bytes.size();
        } else if (nc_payload.type == NC_PAYLOAD_TYPE_FILE) {
          nc_payload.content.file.file_name =
              (char*)payload.GetFileName().c_str();
          nc_payload.content.file.parent_folder =
              (char*)payload.GetParentFolder().c_str();
          nc_payload.content.file.offset = payload.GetOffset();
        } else if (nc_payload.type == NC_PAYLOAD_TYPE_STREAM) {
          // TODO(guogang): support stream later.
        }

        payload_listener.received_callback(
            instance, convertStringToInt(endpoint_id), &nc_payload, context);
      };

  cpp_payload_listener.payload_progress_cb =
      [=](absl::string_view endpoint_id,
          const ::nearby::connections::PayloadProgressInfo& progress) {
        NC_PAYLOAD_PROGRESS_INFO nc_payload_progress_info;
        nc_payload_progress_info.id = progress.payload_id;
        nc_payload_progress_info.bytes_transferred = progress.bytes_transferred;
        nc_payload_progress_info.total_bytes = progress.total_bytes;
        nc_payload_progress_info.status =
            static_cast<NC_PAYLOAD_PROGRESS_INFO_STATUS>(progress.status);
        payload_listener.progress_updated_callback(
            instance, convertStringToInt(endpoint_id),
            &nc_payload_progress_info, context);
      };

  nc_context->core->AcceptConnection(
      convertIntToString(endpoint_id), std::move(cpp_payload_listener),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcRejectConnection(NC_INSTANCE instance, int endpoint_id,
                        NcCallbackResult result_callback,
                        CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->RejectConnection(
      convertIntToString(endpoint_id),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcSendPayload(NC_INSTANCE instance, size_t endpoint_ids_size,
                   const int* endpoint_ids, const NC_PAYLOAD* payload,
                   NcCallbackResult result_callback, CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  std::vector<std::string> endpoint_ids_vector;
  for (size_t i = 0; i < endpoint_ids_size; ++i) {
    endpoint_ids_vector.push_back(convertIntToString(endpoint_ids[i]));
  }

  absl::Span<const std::string> endpoint_ids_span(endpoint_ids_vector.data(),
                                                  endpoint_ids_size);
  ::nearby::connections::Payload cpp_payload;
  if (payload->type == NC_PAYLOAD_TYPE_BYTES) {
    cpp_payload = ::nearby::connections::Payload(
        payload->id, nearby::ByteArray(payload->content.bytes.content.data,
                                       payload->content.bytes.content.size));
  } else if (payload->type == NC_PAYLOAD_TYPE_FILE) {
    // get file size
    std::string full_file_name = "";
    if (payload->content.file.parent_folder == nullptr) {
      full_file_name = payload->content.file.file_name;
    } else {
      full_file_name = absl::StrCat(payload->content.file.parent_folder, "/",
                                    payload->content.file.file_name);
    }

    nearby::InputFile input_file(full_file_name,
                                 getFileSize(full_file_name.c_str()));
    cpp_payload =
        ::nearby::connections::Payload(payload->id, std::move(input_file));
  } else if (payload->type == NC_PAYLOAD_TYPE_STREAM) {
    // TODO(guogang): support stream later.
  }

  nc_context->core->SendPayload(
      endpoint_ids_span, std::move(cpp_payload),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcCancelPayload(NC_INSTANCE instance, NC_PAYLOAD_ID payload_id,
                     NcCallbackResult result_callback, CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->CancelPayload(
      payload_id, [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcDisconnectFromEndpoint(NC_INSTANCE instance, int endpoint_id,
                              NcCallbackResult result_callback,
                              CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->DisconnectFromEndpoint(
      convertIntToString(endpoint_id),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

void NcStopAllEndpoints(NC_INSTANCE instance, NcCallbackResult result_callback,
                        CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->StopAllEndpoints([=](::nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value), context);
  });
}

void NcInitiateBandwidthUpgrade(NC_INSTANCE instance, int endpoint_id,
                                NcCallbackResult result_callback,
                                CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->InitiateBandwidthUpgrade(
      convertIntToString(endpoint_id),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}

int NcGetLocalEndpointId(NC_INSTANCE instance) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    return 0;
  }

  std::string endpoint_id = nc_context->core->GetLocalEndpointId();
  return convertStringToInt(endpoint_id);
}

void NcEnableBleV2(NC_INSTANCE instance, bool enable,
                   NcCallbackResult result_callback, CALLER_CONTEXT context) {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      ::nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableBleV2,
      enable);
  result_callback(NC_STATUS_SUCCESS, context);
}

void NcSetCustomSavePath(NC_INSTANCE instance, const NC_DATA* save_path,
                         NcCallbackResult result_callback,
                         CALLER_CONTEXT context) {
  NcContext* nc_context = GetContext(instance);
  if (nc_context == nullptr) {
    result_callback(NC_STATUS_ERROR, context);
    return;
  }

  nc_context->core->SetCustomSavePath(
      std::string(save_path->data, save_path->size),
      [=](::nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value), context);
      });
}
