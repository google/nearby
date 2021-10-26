// Copyright 2021 Google LLC
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

#include "third_party/dart_lang/v2/runtime/include/dart_api_dl.h"
#include "third_party/nearby_connections/windows/dart/core_adapter_dart.h"

namespace location {
namespace nearby {
namespace connections {

ConnectionOptions g_discovery_connection_options;
ConnectionOptions g_advertising_connection_options;
ABSL_CONST_INIT absl::Mutex callback_mutex(absl::kConstInit);
std::queue<absl::string_view> callback_queue;

Strategy GetStrategy(StrategyDart strategy) {
  switch (strategy) {
    case StrategyDart::P2P_CLUSTER:
      return Strategy::kP2pCluster;
    case StrategyDart::P2P_POINT_TO_POINT:
      return Strategy::kP2pPointToPoint;
    case StrategyDart::P2P_STAR:
      return Strategy::kP2pStar;
  }
  return Strategy::kNone;
}

ByteArray ConvertBluetoothMacAddress(absl::string_view address) {
  return ByteArray();
}

void StartAdvertisingDart(Core* pCore,
                          const char* service_id,
                          StrategyDart strategy,
                          const char* endpoint_info,
                          int auto_upgrade_bandwidth,
                          int enforce_topology_constraints,
                          int enable_bluetooth,
                          int enable_ble,
                          int advertise_nearby_notifications_beacon,
                          int use_low_power_mode,
                          int discover_fast_advertisements,
                          int enable_wifi_lan,
                          int enable_nfc,
                          int enable_wifi_aware,
                          int enable_web_rtc,
                          Dart_Port initiated_cb,
                          Dart_Port accepted_cb,
                          Dart_Port rejected_cb,
                          Dart_Port disconnected_cb,
                          Dart_Port bandwidth_changed_cb,
                          Dart_Port result_cb)
{
  if (pCore) {
    ConnectionOptions options;
    options.strategy = GetStrategy(strategy);
    options.auto_upgrade_bandwidth =
        auto_upgrade_bandwidth;
    options.enforce_topology_constraints =
        enforce_topology_constraints;
    options.allowed.bluetooth = enable_bluetooth;
    options.allowed.ble = enable_ble;
    options.low_power = use_low_power_mode;
    options.fast_advertisement_service_uuid =
     discover_fast_advertisements ? "0000FE2C-0000-1000-8000-00805F9B34FB" : "";
    options.allowed.wifi_lan = enable_wifi_lan;
    options.allowed.web_rtc = enable_web_rtc;

    ConnectionRequestInfo info;
    info.endpoint_info = ByteArray(endpoint_info);
    info.listener.initiated_cb = [initiated_cb](const std::string& endpoint_id,
                          const ConnectionResponseInfo& connection_info) {
                          NEARBY_LOG(INFO, "Advertising initiated: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_initiated;
                          dart_object_initiated.type = Dart_CObject_kString;
                          dart_object_initiated.value.as_string =
                            (char *)connection_info.remote_endpoint_info.data();
                          const bool result =
                              Dart_PostCObject_DL(initiated_cb,
                                                  &dart_object_initiated);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    info.listener.accepted_cb = [accepted_cb](const std::string& endpoint_id) {
                          NEARBY_LOG(INFO, "Advertising accepted: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_accepted;
                          dart_object_accepted.type = Dart_CObject_kString;
                          dart_object_accepted.value.as_string =
                            (char *)endpoint_id.c_str();
                          const bool result =
                              Dart_PostCObject_DL(accepted_cb,
                                                  &dart_object_accepted);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    info.listener.rejected_cb = [rejected_cb](const std::string& endpoint_id,
                                              Status status) {
                          NEARBY_LOG(INFO, "Advertising rejected: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_rejected;
                          dart_object_rejected.type = Dart_CObject_kString;
                          dart_object_rejected.value.as_string =
                            (char *)endpoint_id.c_str();
                          const bool result =
                              Dart_PostCObject_DL(rejected_cb,
                                                  &dart_object_rejected);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    info.listener.disconnected_cb = [disconnected_cb](
        const std::string& endpoint_id) {
                          NEARBY_LOG(INFO, "Advertising disconnected: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_disconnected;
                          dart_object_disconnected.type = Dart_CObject_kString;
                          dart_object_disconnected.value.as_string =
                            (char *)endpoint_id.c_str();
                          const bool result =
                              Dart_PostCObject_DL(disconnected_cb,
                                                  &dart_object_disconnected);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    info.listener.bandwidth_changed_cb = [bandwidth_changed_cb](
        const std::string& endpoint_id, Medium medium) {
                      NEARBY_LOG(INFO, "Advertising bandwidth changed: id=%s",
                                 endpoint_id.c_str());
                      Dart_CObject dart_object_bandwidth_changed;

                      dart_object_bandwidth_changed.type = Dart_CObject_kString;
                      dart_object_bandwidth_changed.value.as_string =
                        (char *)endpoint_id.c_str();
                      const bool result =
                          Dart_PostCObject_DL(bandwidth_changed_cb,
                                              &dart_object_bandwidth_changed);
                      if (!result) {
                        NEARBY_LOG(INFO,
                                   "Posting message to port failed.");
                      }
                    };
    ResultCallback callback;
    callback.result_cb = [result_cb](Status status) {
      NEARBY_LOG(INFO, "Result callback called. %d.", status.value);
      Dart_CObject dart_object_result_callback;
      dart_object_result_callback.type = Dart_CObject_kInt64;
      dart_object_result_callback.value.as_int64 = status.value;
      const bool result = Dart_PostCObject_DL(result_cb,
                                            &dart_object_result_callback);
      if (!result) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
    };

    StartAdvertising(pCore, service_id, options, info, callback);
  }
}

void StopAdvertisingDart(Core* pCore, Dart_Port result_cb) {
  if (pCore) {
    ResultCallback callback;
    callback.result_cb = [result_cb](Status status) {
      NEARBY_LOG(INFO, "Result callback called. %d.", status.value);
      Dart_CObject dart_object_result_callback;
      dart_object_result_callback.type = Dart_CObject_kInt64;
      dart_object_result_callback.value.as_int64 = status.value;
      const bool result = Dart_PostCObject_DL(result_cb,
                                            &dart_object_result_callback);
      if (!result) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
    };
    StopAdvertising(pCore, callback);
  }
}

void StartDiscoveryDart(Core* pCore,
                        const char* service_id,
                        StrategyDart strategy,
                        int forward_unrecognized_bluetooth_devices,
                        int enable_bluetooth,
                        int enable_ble,
                        int use_low_power_mode,
                        int discover_fast_advertisements,
                        int enable_wifi_lan,
                        int enable_nfc,
                        int enable_wifi_aware,
                        Dart_Port found_cb,
                        Dart_Port lost_cb,
                        Dart_Port distance_changed_cb,
                        Dart_Port result_cb) {
  if (pCore) {
    ConnectionOptions options;
    options.strategy = GetStrategy(strategy);
    options.allowed.bluetooth = enable_bluetooth;
    options.allowed.ble = enable_ble;
    options.allowed.wifi_lan = enable_wifi_lan;
    options.allowed.web_rtc = false;
    options.low_power = use_low_power_mode;
    options.enable_bluetooth_listening =
        enable_bluetooth;
    options.enforce_topology_constraints = true;
    options.fast_advertisement_service_uuid =
     discover_fast_advertisements ? "0000FE2C-0000-1000-8000-00805F9B34FB" : "";

    DiscoveryListener listener;
    listener.endpoint_found_cb =
                        [found_cb](const std::string& endpoint_id,
                                 const ByteArray& endpoint_info,
                                 const std::string& str_service_id) {
                          NEARBY_LOG(INFO, "Device discovered: id=%s",
                                     endpoint_id.c_str());
                          NEARBY_LOG(INFO, "Device discovered: service_id=%s",
                                     str_service_id.c_str());
                          NEARBY_LOG(INFO, "Device discovered: info=%s",
                                     ((string)endpoint_info).c_str());
                          Dart_CObject dart_object_found;
                          dart_object_found.type = Dart_CObject_kString;
                          dart_object_found.value.as_string =
                              (char *)endpoint_info.data();
                          const bool result =
                              Dart_PostCObject_DL(found_cb,
                                                  &dart_object_found);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    listener.endpoint_lost_cb =
                        [lost_cb](const std::string& endpoint_id) {
                          NEARBY_LOG(INFO, "Device lost: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_lost;
                          dart_object_lost.type = Dart_CObject_kString;
                          dart_object_lost.value.as_string =
                              (char *)endpoint_id.c_str();
                          const bool result =
                              Dart_PostCObject_DL(lost_cb, &dart_object_lost);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };
    listener.endpoint_distance_changed_cb =
                        [distance_changed_cb](const std::string& endpoint_id,
                                                DistanceInfo info) {
                          NEARBY_LOG(INFO, "Device distance changed: id=%s",
                                     endpoint_id.c_str());
                          Dart_CObject dart_object_distance_changed;
                          dart_object_distance_changed.type =
                              Dart_CObject_kString;
                          dart_object_distance_changed.value.as_string =
                              (char *)(endpoint_id.c_str());
                          const bool result =
                              Dart_PostCObject_DL(distance_changed_cb,
                                              &dart_object_distance_changed);
                          if (!result) {
                            NEARBY_LOG(INFO,
                                       "Posting message to port failed.");
                          }
                        };

    ResultCallback callback;
    callback.result_cb = [result_cb](Status status) {
      NEARBY_LOG(INFO, "Result callback called. %d.", status.value);
      Dart_CObject dart_object_result_callback;
      dart_object_result_callback.type = Dart_CObject_kInt64;
      dart_object_result_callback.value.as_int64 = status.value;
      const bool result = Dart_PostCObject_DL(result_cb,
                                            &dart_object_result_callback);
      if (!result) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
    };

    StartDiscovery(pCore, service_id, options,
                          listener, callback);
  }
}

void StopDiscoveryDart(Core* pCore, Dart_Port result_cb) {
  if (pCore) {
    ResultCallback callback;
    callback.result_cb = [result_cb](Status status) {
      NEARBY_LOG(INFO, "Result callback called. %d.", status.value);
      Dart_CObject dart_object_result_callback;
      dart_object_result_callback.type = Dart_CObject_kInt64;
      dart_object_result_callback.value.as_int64 = status.value;
      const bool result = Dart_PostCObject_DL(result_cb,
                                            &dart_object_result_callback);
      if (!result) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
    };
    StopDiscovery(pCore, callback);
  }
}
}  // namespace connections
}  // namespace nearby
}  // namespace location
