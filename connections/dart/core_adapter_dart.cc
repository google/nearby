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

#include "connections/dart/core_adapter_dart.h"

#include <cstdint>
#include <string>

#include "connections/core.h"
#include "connections/payload.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

static CountDownLatch *adapter_finished;

StrategyW GetStrategy(StrategyDart strategy) {
  switch (strategy) {
    case StrategyDart::P2P_CLUSTER:
      return StrategyW::kP2pCluster;
    case StrategyDart::P2P_POINT_TO_POINT:
      return StrategyW::kP2pPointToPoint;
    case StrategyDart::P2P_STAR:
      return StrategyW::kP2pStar;
  }
  return StrategyW::kNone;
}

ByteArray ConvertBluetoothMacAddress(absl::string_view address) {
  return ByteArray(address.data());
}

static Dart_Port port;
static DiscoveryListenerDart current_discovery_listener_dart;
static ConnectionListenerDart current_connection_listener_dart;
static PayloadListenerDart current_payload_listener_dart;

void ResultCB(Status status) {
  (void)status;  // Avoid unused parameter warning
  Dart_CObject dart_object_result_callback;
  dart_object_result_callback.type = Dart_CObject_kInt64;
  dart_object_result_callback.value.as_int64 = status.value;
  const bool result = Dart_PostCObject_DL(port, &dart_object_result_callback);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
  adapter_finished->CountDown();
}

void ListenerInitiatedCB(
    const char *endpoint_id,
    const ConnectionResponseInfoW &connection_response_info) {
  NEARBY_LOG(INFO, "Advertising initiated: id=%s", endpoint_id);

  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kString;
  dart_object_endpoint_id.value.as_string = const_cast<char *>(endpoint_id);

  Dart_CObject dart_object_endpoint_info;
  dart_object_endpoint_info.type = Dart_CObject_kString;
  dart_object_endpoint_info.value.as_string =
      const_cast<char *>(connection_response_info.remote_endpoint_info);

  Dart_CObject *elements[2];
  elements[0] = &dart_object_endpoint_id;
  elements[1] = &dart_object_endpoint_info;

  Dart_CObject dart_object_initiated;
  dart_object_initiated.type = Dart_CObject_kArray;
  dart_object_initiated.value.as_array.length = 2;
  dart_object_initiated.value.as_array.values = elements;

  const bool result =
      Dart_PostCObject_DL(current_connection_listener_dart.initiated_dart_port,
                          &dart_object_initiated);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerAcceptedCB(const char *endpoint_id) {
  NEARBY_LOG(INFO, "Advertising accepted: id=%s", endpoint_id);
  Dart_CObject dart_object_accepted;
  dart_object_accepted.type = Dart_CObject_kString;
  dart_object_accepted.value.as_string = const_cast<char *>(endpoint_id);
  const bool result =
      Dart_PostCObject_DL(current_connection_listener_dart.accepted_dart_port,
                          &dart_object_accepted);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerRejectedCB(const char *endpoint_id, connections::Status status) {
  NEARBY_LOG(INFO, "Advertising rejected: id=%s", endpoint_id);
  Dart_CObject dart_object_rejected;
  dart_object_rejected.type = Dart_CObject_kString;
  dart_object_rejected.value.as_string = const_cast<char *>(endpoint_id);
  const bool result =
      Dart_PostCObject_DL(current_connection_listener_dart.rejected_dart_port,
                          &dart_object_rejected);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerDisconnectedCB(const char *endpoint_id) {
  NEARBY_LOG(INFO, "Advertising disconnected: id=%s", endpoint_id);
  Dart_CObject dart_object_disconnected;
  dart_object_disconnected.type = Dart_CObject_kString;
  dart_object_disconnected.value.as_string = const_cast<char *>(endpoint_id);
  const bool result = Dart_PostCObject_DL(
      current_connection_listener_dart.disconnected_dart_port,
      &dart_object_disconnected);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerBandwidthChangedCB(const char *endpoint_id, MediumW medium) {
  NEARBY_LOG(INFO, "Advertising bandwidth changed: id=%s", endpoint_id);
  Dart_CObject dart_object_bandwidth_changed;

  dart_object_bandwidth_changed.type = Dart_CObject_kString;
  dart_object_bandwidth_changed.value.as_string =
      const_cast<char *>(endpoint_id);
  const bool result = Dart_PostCObject_DL(
      current_connection_listener_dart.bandwidth_changed_dart_port,
      &dart_object_bandwidth_changed);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerEndpointFoundCB(const char *endpoint_id, const char *endpoint_info,
                             size_t endpoint_info_size,
                             const char *str_service_id) {
  NEARBY_LOG(INFO, "Device discovered: id=%s", endpoint_id);
  NEARBY_LOG(INFO, "Device discovered: service_id=%s", str_service_id);
  NEARBY_LOG(INFO, "Device discovered: info=%s", endpoint_info);

  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kString;
  dart_object_endpoint_id.value.as_string = const_cast<char *>(endpoint_id);

  Dart_CObject dart_object_endpoint_info;
  dart_object_endpoint_info.type = Dart_CObject_kString;
  dart_object_endpoint_info.value.as_string = const_cast<char *>(endpoint_info);

  Dart_CObject *elements[2];
  elements[0] = &dart_object_endpoint_id;
  elements[1] = &dart_object_endpoint_info;

  Dart_CObject dart_object_found;
  dart_object_found.type = Dart_CObject_kArray;
  dart_object_found.value.as_array.length = 2;
  dart_object_found.value.as_array.values = elements;
  const bool result = Dart_PostCObject_DL(
      current_discovery_listener_dart.found_dart_port, &dart_object_found);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}
void ListenerEndpointLostCB(const char *endpoint_id) {
  NEARBY_LOG(INFO, "Device lost: id=%s", endpoint_id);
  Dart_CObject dart_object_lost;
  dart_object_lost.type = Dart_CObject_kString;
  dart_object_lost.value.as_string = const_cast<char *>(endpoint_id);
  const bool result = Dart_PostCObject_DL(
      current_discovery_listener_dart.lost_dart_port, &dart_object_lost);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerEndpointDistanceChangedCB(const char *endpoint_id,
                                       DistanceInfoW distance_info) {
  (void)distance_info;  // Avoid unused parameter warning
  NEARBY_LOG(INFO, "Device distance changed: id=%s", endpoint_id);
  Dart_CObject dart_object_distance_changed;
  dart_object_distance_changed.type = Dart_CObject_kString;
  dart_object_distance_changed.value.as_string =
      const_cast<char *>(endpoint_id);
  const bool result = Dart_PostCObject_DL(
      current_discovery_listener_dart.distance_changed_dart_port,
      &dart_object_distance_changed);
  if (!result) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void ListenerPayloadCB(const char *endpoint_id, PayloadW &payload) {
  NEARBY_LOG(INFO,
             "Payload callback called. id: %s, "
             "payload_id: %d, type: %d, offset: %d",
             endpoint_id, payload.GetId(), payload.GetType(),
             payload.GetOffset());

  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kString;
  dart_object_endpoint_id.value.as_string = const_cast<char *>(endpoint_id);

  Dart_CObject dart_object_payload_id;
  dart_object_payload_id.type = Dart_CObject_kInt64;
  dart_object_payload_id.value.as_int64 = payload.GetId();

  switch (payload.GetType()) {
    case nearby::connections::PayloadType::kBytes: {
      const char *bytes = nullptr;
      size_t bytes_size;

      if (!payload.AsBytes(bytes, bytes_size)) {
        NEARBY_LOG(INFO, "Failed to get the payload as bytes.");
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
              current_payload_listener_dart.initial_byte_info_port,
              &dart_object_payload)) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
      return;
    }
    case nearby::connections::PayloadType::kStream: {
      Dart_CObject *elements[] = {
          &dart_object_endpoint_id,
          &dart_object_payload_id,
      };

      Dart_CObject dart_object_payload;
      dart_object_payload.type = Dart_CObject_kArray;
      dart_object_payload.value.as_array.length = 2;
      dart_object_payload.value.as_array.values = elements;
      if (!Dart_PostCObject_DL(
              current_payload_listener_dart.initial_stream_info_port,
              &dart_object_payload)) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
      return;
    }
    case nearby::connections::PayloadType::kFile: {
      Dart_CObject dart_object_offset;
      dart_object_offset.type = Dart_CObject_kInt64;
      dart_object_offset.value.as_int64 = payload.GetOffset();

      std::string path = payload.AsFile()->GetFilePath();
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
              current_payload_listener_dart.initial_file_info_port,
              &dart_object_payload)) {
        NEARBY_LOG(INFO, "Posting message to port failed.");
      }
      return;
    }
    default:
      NEARBY_LOG(INFO, "Invalid payload type.");
      return;
  }
}

void ListenerPayloadProgressCB(
    const char *endpoint_id,
    const PayloadProgressInfoW &payload_progress_info) {
  NEARBY_LOG(INFO,
             "Payload progress callback called. id: %s, "
             "payload_id: %d, bytes transferred: %d, total: %d, status: %d",
             endpoint_id, payload_progress_info.payload_id,
             payload_progress_info.bytes_transferred,
             payload_progress_info.total_bytes, payload_progress_info.status);
  Dart_CObject dart_object_endpoint_id;
  dart_object_endpoint_id.type = Dart_CObject_kString;
  dart_object_endpoint_id.value.as_string = const_cast<char *>(endpoint_id);

  Dart_CObject dart_object_payload_id;
  dart_object_payload_id.type = Dart_CObject_kInt64;
  dart_object_payload_id.value.as_int64 = payload_progress_info.payload_id;

  Dart_CObject dart_object_bytes_transferred;
  dart_object_bytes_transferred.type = Dart_CObject_kInt64;
  dart_object_bytes_transferred.value.as_int64 =
      payload_progress_info.bytes_transferred;

  Dart_CObject dart_object_total_bytes;
  dart_object_total_bytes.type = Dart_CObject_kInt64;
  dart_object_total_bytes.value.as_int64 = payload_progress_info.total_bytes;

  Dart_CObject dart_object_status;
  dart_object_status.type = Dart_CObject_kInt64;
  dart_object_status.value.as_int64 = (int64_t)payload_progress_info.status;

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
          current_payload_listener_dart.payload_progress_dart_port,
          &dart_object_payload_progress)) {
    NEARBY_LOG(INFO, "Posting message to port failed.");
  }
}

void SetResultCallback(ResultCallbackW &result_callback, Dart_Port &dart_port) {
  port = dart_port;
  result_callback.result_cb = ResultCB;
}

void PostResult(Dart_Port &result_cb, Status::Value value) {
  port = result_cb;
  Dart_CObject dart_object_result_callback;
  dart_object_result_callback.type = Dart_CObject_kInt64;
  dart_object_result_callback.value.as_int64 = value;
  const bool result =
      Dart_PostCObject_DL(result_cb, &dart_object_result_callback);
  if (!result) {
    NEARBY_LOG(INFO, "Returning error to port failed.");
  }
}

void EnableBleV2Dart(Core *pCore, int64_t enable, Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }
  port = result_cb;

  FeatureFlags::GetMutableFlagsForTesting().support_ble_v2 = enable;
  PostResult(result_cb, Status::kSuccess);
}

void StartAdvertisingDart(
    Core *pCore, const char *service_id,
    AdvertisingOptionsDart connection_options_dart,
    ConnectionRequestInfoDart connection_request_info_dart,
    Dart_Port result_cb) {
  if (!pCore) {
    PostResult(result_cb, Status::Value::kError);
    return;
  }

  port = result_cb;
  current_connection_listener_dart =
      connection_request_info_dart.connection_listener;

  AdvertisingOptionsW advertising_options;
  advertising_options.strategy = GetStrategy(connection_options_dart.strategy);
  advertising_options.auto_upgrade_bandwidth =
      connection_options_dart.auto_upgrade_bandwidth;
  advertising_options.enforce_topology_constraints =
      connection_options_dart.enforce_topology_constraints;

  advertising_options.low_power = connection_options_dart.low_power;
  advertising_options.fast_advertisement_service_uuid =
      connection_options_dart.fast_advertisement_service_uuid;

  advertising_options.allowed.bluetooth =
      connection_options_dart.mediums.bluetooth;
  advertising_options.allowed.ble = connection_options_dart.mediums.ble;
  advertising_options.allowed.wifi_lan =
      connection_options_dart.mediums.wifi_lan;
  advertising_options.allowed.wifi_hotspot =
      connection_options_dart.mediums.wifi_hotspot;
  advertising_options.allowed.web_rtc = connection_options_dart.mediums.web_rtc;

  ConnectionListenerW listener(ListenerInitiatedCB, ListenerAcceptedCB,
                               ListenerRejectedCB, ListenerDisconnectedCB,
                               ListenerBandwidthChangedCB);

  ConnectionRequestInfoW info{
      connection_request_info_dart.endpoint_info,
      strlen(connection_request_info_dart.endpoint_info), listener};

  ResultCallbackW callback;
  SetResultCallback(callback, result_cb);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  StartAdvertising(pCore, service_id, advertising_options, info, callback);

  finished.Await();
}

void StopAdvertisingDart(Core *pCore, Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  StopAdvertising(pCore, callback);

  finished.Await();
}

void StartDiscoveryDart(Core *pCore, const char *service_id,
                        DiscoveryOptionsDart discovery_options_dart,
                        DiscoveryListenerDart discovery_listener_dart,
                        Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  current_discovery_listener_dart = discovery_listener_dart;

  DiscoveryOptionsW discovery_options;
  discovery_options.strategy = GetStrategy(discovery_options_dart.strategy);
  discovery_options.allowed.web_rtc = false;
  discovery_options.enforce_topology_constraints = true;
  // This needs to be passed in by the UI. If it's null, then no
  // fast_advertisement_service. Otherwise this interface will always
  // and forever be locked into 0000FE2C-0000-1000-8000-00805F9B34FB
  // whenever fast advertisement service is requested.
  discovery_options.fast_advertisement_service_uuid =
      discovery_options_dart.fast_advertisement_service_uuid;

  discovery_options.allowed.bluetooth =
      discovery_options_dart.mediums.bluetooth;
  discovery_options.allowed.ble = discovery_options_dart.mediums.ble;
  discovery_options.allowed.wifi_lan = discovery_options_dart.mediums.wifi_lan;
  discovery_options.allowed.wifi_hotspot =
      discovery_options_dart.mediums.wifi_hotspot;
  discovery_options.allowed.web_rtc = discovery_options_dart.mediums.web_rtc;

  DiscoveryListenerW listener(ListenerEndpointFoundCB, ListenerEndpointLostCB,
                              ListenerEndpointDistanceChangedCB);

  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  StartDiscovery(pCore, service_id, discovery_options, listener, callback);

  finished.Await();
}

void StopDiscoveryDart(Core *pCore, Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  StopDiscovery(pCore, callback);

  adapter_finished->Await();
}

void RequestConnectionDart(
    Core *pCore, const char *endpoint_id,
    ConnectionOptionsDart connection_options_dart,
    ConnectionRequestInfoDart connection_request_info_dart,
    Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  current_connection_listener_dart =
      connection_request_info_dart.connection_listener;

  ConnectionOptionsW connection_options;
  connection_options.enforce_topology_constraints = false;
  connection_options.remote_bluetooth_mac_address =
      connection_options_dart.remote_bluetooth_mac_address;
  connection_options.fast_advertisement_service_uuid =
      connection_options_dart.fast_advertisement_service_uuid;
  connection_options.keep_alive_interval_millis =
      connection_options_dart.keep_alive_interval_millis;
  connection_options.keep_alive_timeout_millis =
      connection_options_dart.keep_alive_timeout_millis;
  connection_options.allowed.bluetooth =
      connection_options_dart.mediums.bluetooth;
  connection_options.allowed.ble = connection_options_dart.mediums.ble;
  connection_options.allowed.wifi_lan =
      connection_options_dart.mediums.wifi_lan;
  connection_options.allowed.wifi_hotspot =
      connection_options_dart.mediums.wifi_hotspot;
  connection_options.allowed.web_rtc = connection_options_dart.mediums.web_rtc;

  ConnectionListenerW listener(ListenerInitiatedCB, ListenerAcceptedCB,
                               ListenerRejectedCB, ListenerDisconnectedCB,
                               ListenerBandwidthChangedCB);

  ConnectionRequestInfoW info{
      connection_request_info_dart.endpoint_info,
      strlen(connection_request_info_dart.endpoint_info), listener};

  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  RequestConnection(pCore, endpoint_id, info, connection_options, callback);

  adapter_finished->Await();
}

void AcceptConnectionDart(Core *pCore, const char *endpoint_id,
                          PayloadListenerDart payload_listener_dart,
                          Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  current_payload_listener_dart = payload_listener_dart;

  PayloadListenerW listener(ListenerPayloadCB, ListenerPayloadProgressCB);

  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  AcceptConnection(pCore, endpoint_id, listener, callback);

  finished.Await();
}

void RejectConnectionDart(Core *pCore, const char *endpoint_id,
                          Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;

  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  RejectConnection(pCore, endpoint_id, callback);

  finished.Await();
}
void DisconnectFromEndpointDart(Core *pCore, char *endpoint_id,
                                Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;
  ResultCallbackW callback;
  SetResultCallback(callback, dart_port);

  CountDownLatch finished(1);
  adapter_finished = &finished;

  DisconnectFromEndpoint(pCore, endpoint_id, callback);

  finished.Await();
}

void SendPayloadDart(Core *pCore, const char *endpoint_id,
                     PayloadDart payload_dart, Dart_Port dart_port) {
  if (!pCore) {
    PostResult(dart_port, Status::Value::kError);
    return;
  }

  port = dart_port;

  ResultCallbackW callback;
  std::vector<std::string> endpoint_ids = {std::string(endpoint_id)};

  NEARBY_LOG(INFO, "Payload type: %d", payload_dart.type);
  switch (payload_dart.type) {
    case UNKNOWN:
    case STREAM:
      NEARBY_LOG(INFO, "Payload type not supported yet");
      PostResult(dart_port, Status::Value::kPayloadUnknown);
      break;
    case BYTE: {
      PayloadW payload(PayloadW::GenerateId(), payload_dart.data,
                       payload_dart.size);

      std::vector<const char *> c_string_array;

      std::transform(endpoint_ids.begin(), endpoint_ids.end(),
                     std::back_inserter(c_string_array),
                     [](const std::string &s) {
                       char *pc = new char[s.size() + 1];
                       strncpy(pc, s.c_str(), s.size() + 1);
                       return pc;
                     });

      SetResultCallback(callback, dart_port);

      CountDownLatch finished(1);
      adapter_finished = &finished;

      SendPayload(pCore, c_string_array.data(), c_string_array.size(),
                  std::move(payload), callback);

      adapter_finished->Await();
    } break;
    case FILE:
      NEARBY_LOG(INFO, "File name: %s, size %d", payload_dart.data,
                 payload_dart.size);
      std::string file_name_str(payload_dart.data);
      InputFileW input_file(file_name_str.c_str(), payload_dart.size);
      PayloadW payload(input_file);

      std::vector<const char *> c_string_array;

      std::transform(endpoint_ids.begin(), endpoint_ids.end(),
                     std::back_inserter(c_string_array),
                     [](const std::string &s) {
                       char *pc = new char[s.size() + 1];
                       strncpy(pc, s.c_str(), s.size() + 1);
                       return pc;
                     });

      SetResultCallback(callback, dart_port);

      CountDownLatch finished(1);
      adapter_finished = &finished;

      SendPayload(pCore, c_string_array.data(), c_string_array.size(),
                  std::move(payload), callback);

      adapter_finished->Await();

      break;
  }
}

}  // namespace nearby::windows
