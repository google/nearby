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

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
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
  nearby::connections::ServiceControllerRouter* router = nullptr;
  nearby::connections::Core* core = nullptr;
} NcContext;

static NcContext kNcContext;

int64_t getFileSize(const char* filename) {
  struct stat file_status;
  if (stat(filename, &file_status) < 0) {
    return -1;
  }

  return file_status.st_size;
}

nearby::connections::ConnectionRequestInfo GetCppConnectionRequestInfo(
    const NC_CONNECTION_REQUEST_INFO& connection_request_info) {
  nearby::connections::ConnectionRequestInfo cpp_connection_request_info;
  cpp_connection_request_info.endpoint_info =
      nearby::ByteArray(connection_request_info.endpoint_info.data,
                        connection_request_info.endpoint_info.size);
  nearby::connections::ConnectionListener cpp_connection_listener;
  cpp_connection_listener.accepted_cb = [=](const std::string& endpoint_id) {
    connection_request_info.accepted_callback(endpoint_id.c_str());
  };
  cpp_connection_listener.bandwidth_changed_cb =
      [=](const std::string& endpoint_id, nearby::connections::Medium medium) {
        connection_request_info.bandwidth_changed_callback(
            endpoint_id.c_str(), static_cast<NC_MEDIUM>(medium));
      };
  cpp_connection_listener.disconnected_cb =
      [=](const std::string& endpoint_id) {
        connection_request_info.disconnected_callback(endpoint_id.c_str());
      };
  cpp_connection_listener.initiated_cb =
      [=](const std::string& endpoint_id,
          const nearby::connections::ConnectionResponseInfo& info) {
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

        connection_request_info.initiated_callback(endpoint_id.c_str(),
                                                   connection_response_info);
      };

  cpp_connection_listener.rejected_cb =
      [=](const std::string& endpoint_id, nearby::connections::Status status) {
        connection_request_info.rejected_callback(
            endpoint_id.c_str(), static_cast<NC_STATUS>(status.value));
      };

  cpp_connection_request_info.listener = std::move(cpp_connection_listener);
  return cpp_connection_request_info;
}

NC_INSTANCE NcOpenService() {
  if (kNcContext.core == nullptr) {
    kNcContext.router = new nearby::connections::ServiceControllerRouter();
    kNcContext.core = new nearby::connections::Core(kNcContext.router);
  }

  return kNcContext.core;
}

void NcCloseService(NC_INSTANCE instance) {
  if (kNcContext.core == nullptr) {
    return;
  }

  kNcContext.core->StopAllEndpoints([](nearby::connections::Status status) {
    NEARBY_LOGS(INFO) << "Stopping all endpoints with status "
                      << status.ToString();
  });
  delete kNcContext.router;
  delete kNcContext.core;
  kNcContext.router = nullptr;
  kNcContext.core = nullptr;
}

void NcStartAdvertising(
    NC_INSTANCE instance, const char* service_id,
    const NC_ADVERTISING_OPTIONS& advertising_options,
    const NC_CONNECTION_REQUEST_INFO& connection_request_info,
    NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  nearby::connections::ConnectionRequestInfo cpp_connection_request_info =
      GetCppConnectionRequestInfo(connection_request_info);

  nearby::connections::AdvertisingOptions cpp_advertising_options;
  cpp_advertising_options.allowed.ble =
      advertising_options.common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_advertising_options.allowed.bluetooth =
      advertising_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_advertising_options.allowed.wifi_lan =
      advertising_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_advertising_options.allowed.wifi_direct =
      advertising_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_DIRECT];
  cpp_advertising_options.allowed.wifi_hotspot =
      advertising_options.common_options
          .allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT];
  cpp_advertising_options.allowed.web_rtc =
      advertising_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];
  cpp_advertising_options.enable_bluetooth_listening =
      advertising_options.enable_bluetooth_listening;
  cpp_advertising_options.enable_webrtc_listening =
      advertising_options.enable_webrtc_listening;
  cpp_advertising_options.auto_upgrade_bandwidth =
      advertising_options.auto_upgrade_bandwidth;
  cpp_advertising_options.enforce_topology_constraints =
      advertising_options.enforce_topology_constraints;
  if (advertising_options.fast_advertisement_service_uuid.size > 0) {
    cpp_advertising_options.fast_advertisement_service_uuid =
        std::string(advertising_options.fast_advertisement_service_uuid.data,
                    advertising_options.fast_advertisement_service_uuid.size);
  }

  cpp_advertising_options.is_out_of_band_connection =
      advertising_options.is_out_of_band_connection;
  cpp_advertising_options.low_power = advertising_options.low_power;

  if (advertising_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_NONE) {
    cpp_advertising_options.strategy = nearby::connections::Strategy::kNone;
  }
  if (advertising_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_advertising_options.strategy =
        nearby::connections::Strategy::kP2pCluster;
  if (advertising_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_advertising_options.strategy =
        nearby::connections::Strategy::kP2pPointToPoint;
  if (advertising_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_advertising_options.strategy = nearby::connections::Strategy::kP2pStar;

  kNcContext.core->StartAdvertising(
      service_id, std::move(cpp_advertising_options),
      std::move(cpp_connection_request_info),
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcStopAdvertising(NC_INSTANCE instance, NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->StopAdvertising([=](nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value));
  });
}

void NcStartDiscovery(NC_INSTANCE instance, const char* service_id,
                      const NC_DISCOVERY_OPTIONS& discovery_options,
                      const NC_DISCOVERY_LISTENER& discovery_listener,
                      NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  nearby::connections::DiscoveryOptions cpp_discovery_options;

  if (discovery_options.common_options.strategy.type == NC_STRATEGY_TYPE_NONE)
    cpp_discovery_options.strategy = nearby::connections::Strategy::kNone;
  if (discovery_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_discovery_options.strategy = nearby::connections::Strategy::kP2pCluster;
  if (discovery_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_discovery_options.strategy =
        nearby::connections::Strategy::kP2pPointToPoint;
  if (discovery_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_discovery_options.strategy = nearby::connections::Strategy::kP2pStar;
  cpp_discovery_options.auto_upgrade_bandwidth =
      discovery_options.auto_upgrade_bandwidth;
  cpp_discovery_options.enforce_topology_constraints =
      discovery_options.enforce_topology_constraints;
  cpp_discovery_options.is_out_of_band_connection =
      discovery_options.is_out_of_band_connection;
  if (discovery_options.fast_advertisement_service_uuid.size > 0) {
    cpp_discovery_options.fast_advertisement_service_uuid =
        std::string(discovery_options.fast_advertisement_service_uuid.data,
                    discovery_options.fast_advertisement_service_uuid.size);
  }
  cpp_discovery_options.allowed.bluetooth =
      discovery_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_discovery_options.allowed.ble =
      discovery_options.common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_discovery_options.allowed.wifi_lan =
      discovery_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_discovery_options.allowed.wifi_hotspot =
      discovery_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_HOTSPOT];
  cpp_discovery_options.allowed.web_rtc =
      discovery_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];

  nearby::connections::DiscoveryListener listener;
  listener.endpoint_distance_changed_cb =
      [=](const std::string& endpoint_id,
          nearby::connections::DistanceInfo info) {
        discovery_listener.endpoint_distance_changed_callback(
            endpoint_id.c_str(), static_cast<NC_DISTANCE_INFO>(info));
      };
  listener.endpoint_found_cb = [=](const std::string& endpoint_id,
                                   const nearby::ByteArray& endpoint_info,
                                   const std::string& service_id) {
    discovery_listener.endpoint_found_callback(
        endpoint_id.c_str(),
        NC_DATA{.size = endpoint_info.size(),
                .data = (char*)endpoint_info.data()},
        service_id.c_str());
  };

  listener.endpoint_lost_cb = [=](const std::string& endpoint_id) {
    discovery_listener.endpoint_lost_callback(endpoint_id.c_str());
  };

  kNcContext.core->StartDiscovery(
      service_id, std::move(cpp_discovery_options), std::move(listener),
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcStopDiscovery(NC_INSTANCE instance, NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->StopDiscovery([=](nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value));
  });
}

void NcInjectEndpoint(NC_INSTANCE instance, const char* service_id,
                      const NC_OUT_OF_BAND_CONNECTION_METADATA& metadata,
                      NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }
  nearby::connections::OutOfBandConnectionMetadata
      cpp_out_of_band_connection_metadata;
  cpp_out_of_band_connection_metadata.endpoint_id = metadata.endpoint_id;
  cpp_out_of_band_connection_metadata.endpoint_info = {
      metadata.endpoint_info.data, metadata.endpoint_info.size};
  cpp_out_of_band_connection_metadata.medium =
      static_cast<nearby::connections::Medium>(metadata.medium);
  cpp_out_of_band_connection_metadata.remote_bluetooth_mac_address = {
      metadata.remote_bluetooth_mac_address.data,
      metadata.remote_bluetooth_mac_address.size};

  kNcContext.core->InjectEndpoint(
      service_id, cpp_out_of_band_connection_metadata,
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcRequestConnection(
    NC_INSTANCE instance, const char* endpoint_id,
    const NC_CONNECTION_REQUEST_INFO& connection_request_info,
    const NC_CONNECTION_OPTIONS& connection_options,
    NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  nearby::connections::ConnectionRequestInfo cpp_connection_request_info =
      GetCppConnectionRequestInfo(connection_request_info);

  nearby::connections::ConnectionOptions cpp_connection_options;
  cpp_connection_options.allowed.ble =
      connection_options.common_options.allowed_mediums[NC_MEDIUM_BLE];
  cpp_connection_options.allowed.bluetooth =
      connection_options.common_options.allowed_mediums[NC_MEDIUM_BLUETOOTH];
  cpp_connection_options.allowed.web_rtc =
      connection_options.common_options.allowed_mediums[NC_MEDIUM_WEB_RTC];
  cpp_connection_options.allowed.wifi_lan =
      connection_options.common_options.allowed_mediums[NC_MEDIUM_WIFI_LAN];
  cpp_connection_options.auto_upgrade_bandwidth =
      connection_options.auto_upgrade_bandwidth;
  cpp_connection_options.enforce_topology_constraints =
      connection_options.enforce_topology_constraints;
  if (connection_options.fast_advertisement_service_uuid.size > 0) {
    cpp_connection_options.fast_advertisement_service_uuid =
        std::string(connection_options.fast_advertisement_service_uuid.data,
                    connection_options.fast_advertisement_service_uuid.size);
  }
  cpp_connection_options.is_out_of_band_connection =
      connection_options.is_out_of_band_connection;
  cpp_connection_options.keep_alive_interval_millis =
      connection_options.keep_alive_interval_millis;
  cpp_connection_options.keep_alive_timeout_millis =
      connection_options.keep_alive_timeout_millis;
  cpp_connection_options.low_power = connection_options.low_power;
  if (connection_options.remote_bluetooth_mac_address.size > 0) {
    cpp_connection_options.remote_bluetooth_mac_address =
        nearby::BluetoothUtils::FromString(
            std::string(connection_options.remote_bluetooth_mac_address.data,
                        connection_options.remote_bluetooth_mac_address.size));
  }
  if (connection_options.common_options.strategy.type == NC_STRATEGY_TYPE_NONE)
    cpp_connection_options.strategy = nearby::connections::Strategy::kNone;
  if (connection_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_CLUSTER)
    cpp_connection_options.strategy =
        nearby::connections::Strategy::kP2pCluster;
  if (connection_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_POINT_TO_POINT)
    cpp_connection_options.strategy =
        nearby::connections::Strategy::kP2pPointToPoint;
  if (connection_options.common_options.strategy.type ==
      NC_STRATEGY_TYPE_P2P_STAR)
    cpp_connection_options.strategy = nearby::connections::Strategy::kP2pStar;

  kNcContext.core->RequestConnection(
      endpoint_id, std::move(cpp_connection_request_info),
      std::move(cpp_connection_options),
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcAcceptConnection(NC_INSTANCE instance, const char* endpoint_id,
                        NC_PAYLOAD_LISTENER payload_listener,
                        NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  nearby::connections::PayloadListener cpp_payload_listener;
  cpp_payload_listener.payload_cb = [=](absl::string_view endpoint_id,
                                        nearby::connections::Payload payload) {
    NC_PAYLOAD nc_payload;
    nc_payload.id = payload.GetId();
    nc_payload.direction = NC_PAYLOAD_DIRECTION_INCOMING;
    nc_payload.type = static_cast<NC_PAYLOAD_TYPE>(payload.GetType());
    if (nc_payload.type == NC_PAYLOAD_TYPE_BYTES) {
      nearby::ByteArray bytes = payload.AsBytes();
      nc_payload.content.bytes.content.data = bytes.data();
      nc_payload.content.bytes.content.size = bytes.size();
    } else if (nc_payload.type == NC_PAYLOAD_TYPE_FILE) {
      nc_payload.content.file.file_name = (char*)payload.GetFileName().c_str();
      nc_payload.content.file.parent_folder =
          (char*)payload.GetParentFolder().c_str();
      nc_payload.content.file.offset = payload.GetOffset();
    } else if (nc_payload.type == NC_PAYLOAD_TYPE_STREAM) {
      // TODO(guogang): support stream later.
    }

    payload_listener.received_callback(std::string(endpoint_id).c_str(),
                                       nc_payload);
  };

  cpp_payload_listener.payload_progress_cb =
      [=](absl::string_view endpoint_id,
          const nearby::connections::PayloadProgressInfo& progress) {
        NC_PAYLOAD_PROGRESS_INFO nc_payload_progress_info;
        nc_payload_progress_info.id = progress.payload_id;
        nc_payload_progress_info.bytes_transferred = progress.bytes_transferred;
        nc_payload_progress_info.total_bytes = progress.total_bytes;
        nc_payload_progress_info.status =
            static_cast<NC_PAYLOAD_PROGRESS_INFO_STATUS>(progress.status);
        payload_listener.progress_updated_callback(
            std::string(endpoint_id).c_str(), nc_payload_progress_info);
      };

  kNcContext.core->AcceptConnection(
      endpoint_id, std::move(cpp_payload_listener),
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcRejectConnection(NC_INSTANCE instance, const char* endpoint_id,
                        NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->RejectConnection(
      endpoint_id, [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcSendPayload(NC_INSTANCE instance, size_t endpoint_ids_size,
                   const char** endpoint_ids, const NC_PAYLOAD& payload,
                   NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  std::vector<std::string> endpoint_ids_vector;
  for (size_t i = 0; i < endpoint_ids_size; ++i) {
    endpoint_ids_vector.push_back(std::string(endpoint_ids[i]));
  }

  absl::Span<const std::string> endpoint_ids_span(endpoint_ids_vector.data(),
                                                  endpoint_ids_size);
  nearby::connections::Payload cpp_payload;
  if (payload.type == NC_PAYLOAD_TYPE_BYTES) {
    cpp_payload = nearby::connections::Payload(
        payload.id, nearby::ByteArray(payload.content.bytes.content.data,
                                      payload.content.bytes.content.size));
  } else if (payload.type == NC_PAYLOAD_TYPE_FILE) {
    // get file size
    std::string full_file_name = "";
    if (payload.content.file.parent_folder == nullptr) {
      full_file_name = payload.content.file.file_name;
    } else {
      full_file_name = absl::StrCat(payload.content.file.parent_folder, "/",
                                    payload.content.file.file_name);
    }

    nearby::InputFile input_file(full_file_name,
                                 getFileSize(full_file_name.c_str()));
    cpp_payload =
        nearby::connections::Payload(payload.id, std::move(input_file));
  } else if (payload.type == NC_PAYLOAD_TYPE_STREAM) {
    // TODO(guogang): support stream later.
  }

  kNcContext.core->SendPayload(
      endpoint_ids_span, std::move(cpp_payload),
      [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcCancelPayload(NC_INSTANCE instance, NC_PAYLOAD_ID payload_id,
                     NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->CancelPayload(
      payload_id, [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcDisconnectFromEndpoint(NC_INSTANCE instance, const char* endpoint_id,
                              NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->DisconnectFromEndpoint(
      endpoint_id, [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

void NcStopAllEndpoints(NC_INSTANCE instance,
                        NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->StopAllEndpoints([=](nearby::connections::Status status) {
    result_callback(static_cast<NC_STATUS>(status.value));
  });
}

void NcInitiateBandwidthUpgrade(NC_INSTANCE instance, const char* endpoint_id,
                                NcCallbackResult result_callback) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    result_callback(NC_STATUS_ERROR);
    return;
  }

  kNcContext.core->InitiateBandwidthUpgrade(
      endpoint_id, [=](nearby::connections::Status status) {
        result_callback(static_cast<NC_STATUS>(status.value));
      });
}

char* NcGetLocalEndpointId(NC_INSTANCE instance) {
  if (kNcContext.core == nullptr || instance != kNcContext.core) {
    return nullptr;
  }

  std::string endpoint_id = kNcContext.core->GetLocalEndpointId();
  char* result = new char[endpoint_id.length() + 1];
  absl::SNPrintF(result, endpoint_id.length() + 1, "%s", endpoint_id);
  return result;
}

void NcEnableBleV2(NC_INSTANCE instance, bool enable,
                   NcCallbackResult result_callback) {
  nearby::NearbyFlags::GetInstance().OverrideBoolFlagValue(
      nearby::connections::config_package_nearby::nearby_connections_feature::
          kEnableBleV2,
      enable);
  result_callback(NC_STATUS_SUCCESS);
}
