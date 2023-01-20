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

#include "connections/c/core_adapter.h"

#include "absl/strings/str_format.h"
#include "connections/core.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

Core *InitCore(connections::ServiceControllerRouter *router) {
#if defined(LOG_SEVERITY_VERBOSE)
  NEARBY_LOG_SET_SEVERITY(VERBOSE);
#endif  // LOG_SEVERITY_VERBOSE;
  return new nearby::connections::Core(router);
}

void CloseCore(Core *pCore) {
  if (pCore == nullptr) {
    return;
  }
  pCore->StopAllEndpoints(
      {.result_cb = std::function<void(Status)>{[](Status) {}}});
  delete pCore;
}

void StartAdvertising(Core *pCore, const char *service_id,
                      AdvertisingOptionsW advertising_options_w,
                      ConnectionRequestInfoW info, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  connections::ConnectionRequestInfo crInfo;

  crInfo.endpoint_info = ByteArray(info.endpoint_info);
  crInfo.listener = std::move(*(info.listener.GetImpl()));

  connections::AdvertisingOptions advertising_options;

  advertising_options.allowed.bluetooth =
      advertising_options_w.allowed.bluetooth;
  advertising_options.allowed.ble = advertising_options_w.allowed.ble;
  advertising_options.allowed.wifi_lan = advertising_options_w.allowed.wifi_lan;
  advertising_options.allowed.web_rtc = advertising_options_w.allowed.web_rtc;
  advertising_options.allowed.wifi_hotspot =
      advertising_options_w.allowed.wifi_hotspot;
  advertising_options.enable_bluetooth_listening = false;
  advertising_options.enable_webrtc_listening = false;
  advertising_options.auto_upgrade_bandwidth =
      advertising_options_w.auto_upgrade_bandwidth;
  advertising_options.enforce_topology_constraints =
      advertising_options_w.enforce_topology_constraints;
  if (advertising_options_w.fast_advertisement_service_uuid != nullptr) {
    advertising_options.fast_advertisement_service_uuid =
        std::string(advertising_options_w.fast_advertisement_service_uuid);
  }
  advertising_options.is_out_of_band_connection =
      advertising_options_w.is_out_of_band_connection;
  advertising_options.low_power = advertising_options_w.low_power;
  if (advertising_options_w.strategy == StrategyW::kNone)
    advertising_options.strategy = connections::Strategy::kNone;
  if (advertising_options_w.strategy == StrategyW::kP2pCluster)
    advertising_options.strategy = connections::Strategy::kP2pCluster;
  if (advertising_options_w.strategy == StrategyW::kP2pPointToPoint)
    advertising_options.strategy = connections::Strategy::kP2pPointToPoint;
  if (advertising_options_w.strategy == StrategyW::kP2pStar)
    advertising_options.strategy = connections::Strategy::kP2pStar;

  pCore->StartAdvertising(service_id, advertising_options, crInfo,
                          *callback.GetImpl());
}

void StopAdvertising(connections::Core *pCore, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->StopAdvertising(*callback.GetImpl());
}

void StartDiscovery(connections::Core *pCore, const char *service_id,
                    DiscoveryOptionsW discovery_options_w,
                    DiscoveryListenerW listener, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  connections::DiscoveryListener discoveryListener =
      std::move(*listener.GetImpl());

  connections::DiscoveryOptions discovery_options;

  if (discovery_options_w.strategy == StrategyW::kNone)
    discovery_options.strategy = connections::Strategy::kNone;
  if (discovery_options_w.strategy == StrategyW::kP2pCluster)
    discovery_options.strategy = connections::Strategy::kP2pCluster;
  if (discovery_options_w.strategy == StrategyW::kP2pPointToPoint)
    discovery_options.strategy = connections::Strategy::kP2pPointToPoint;
  if (discovery_options_w.strategy == StrategyW::kP2pStar)
    discovery_options.strategy = connections::Strategy::kP2pStar;
  discovery_options.auto_upgrade_bandwidth =
      discovery_options_w.auto_upgrade_bandwidth;
  discovery_options.enforce_topology_constraints =
      discovery_options_w.enforce_topology_constraints;
  discovery_options.keep_alive_interval_millis =
      discovery_options_w.keep_alive_interval_millis;
  discovery_options.keep_alive_timeout_millis =
      discovery_options_w.keep_alive_timeout_millis;
  discovery_options.is_out_of_band_connection =
      discovery_options_w.is_out_of_band_connection;
  if (discovery_options_w.fast_advertisement_service_uuid) {
    discovery_options.fast_advertisement_service_uuid =
        std::string(discovery_options_w.fast_advertisement_service_uuid);
  }
  discovery_options.allowed.bluetooth = discovery_options_w.allowed.bluetooth;
  discovery_options.allowed.ble = discovery_options_w.allowed.ble;
  discovery_options.allowed.wifi_lan = discovery_options_w.allowed.wifi_lan;
  discovery_options.allowed.wifi_hotspot =
      discovery_options_w.allowed.wifi_hotspot;
  discovery_options.allowed.web_rtc = discovery_options_w.allowed.web_rtc;

  pCore->StartDiscovery(service_id, discovery_options, discoveryListener,
                        std::move(*callback.GetImpl()));
}

void StopDiscovery(connections::Core *pCore, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->StopDiscovery(*callback.GetImpl());
}

void InjectEndpoint(connections::Core *pCore, char *service_id,
                    OutOfBandConnectionMetadataW metadata,
                    ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  connections::OutOfBandConnectionMetadata outOfBandConnectionMetadata;
  outOfBandConnectionMetadata.endpoint_id = metadata.endpoint_id;
  outOfBandConnectionMetadata.endpoint_info = {metadata.endpoint_info,
                                               metadata.endpoint_info_size};
  outOfBandConnectionMetadata.medium = metadata.medium;
  outOfBandConnectionMetadata.remote_bluetooth_mac_address = {
      metadata.remote_bluetooth_mac_address,
      metadata.remote_bluetooth_mac_address_size};

  pCore->InjectEndpoint(service_id, outOfBandConnectionMetadata,
                        *callback.GetImpl());
}

void RequestConnection(connections::Core *pCore, const char *endpoint_id,
                       ConnectionRequestInfoW info,
                       ConnectionOptionsW connection_options_w,
                       ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  connections::ConnectionRequestInfo connectionRequestInfo =
      connections::ConnectionRequestInfo();
  connectionRequestInfo.endpoint_info = ByteArray(info.endpoint_info);
  connectionRequestInfo.listener = std::move(*info.listener.GetImpl());

  connections::ConnectionOptions connection_options;
  connection_options.allowed.ble = connection_options_w.allowed.ble;
  connection_options.allowed.bluetooth = connection_options_w.allowed.bluetooth;
  connection_options.allowed.web_rtc = connection_options_w.allowed.web_rtc;
  connection_options.allowed.wifi_lan = connection_options_w.allowed.wifi_lan;
  connection_options.auto_upgrade_bandwidth =
      connection_options_w.auto_upgrade_bandwidth;
  connection_options.enforce_topology_constraints =
      connection_options_w.enforce_topology_constraints;
  if (connection_options_w.fast_advertisement_service_uuid) {
    connection_options.fast_advertisement_service_uuid =
        std::string(connection_options_w.fast_advertisement_service_uuid);
  }
  connection_options.is_out_of_band_connection =
      connection_options_w.is_out_of_band_connection;
  connection_options.keep_alive_interval_millis =
      connection_options_w.keep_alive_interval_millis;
  connection_options.keep_alive_timeout_millis =
      connection_options_w.keep_alive_timeout_millis;
  connection_options.low_power = connection_options_w.low_power;
  if (connection_options_w.remote_bluetooth_mac_address) {
    connection_options.remote_bluetooth_mac_address =
        BluetoothUtils::FromString(
            connection_options_w.remote_bluetooth_mac_address);
  }
  if (connection_options_w.strategy == StrategyW::kNone)
    connection_options.strategy = connections::Strategy::kNone;
  if (connection_options_w.strategy == StrategyW::kP2pCluster)
    connection_options.strategy = connections::Strategy::kP2pCluster;
  if (connection_options_w.strategy == StrategyW::kP2pPointToPoint)
    connection_options.strategy = connections::Strategy::kP2pPointToPoint;
  if (connection_options_w.strategy == StrategyW::kP2pStar)
    connection_options.strategy = connections::Strategy::kP2pStar;

  pCore->RequestConnection(endpoint_id, connectionRequestInfo,
                           connection_options, *callback.GetImpl());
}

void AcceptConnection(connections::Core *pCore, const char *endpoint_id,
                      PayloadListenerW listener, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  connections::PayloadListener payload_listener =
      std::move(*listener.GetImpl());
  pCore->AcceptConnection(endpoint_id, payload_listener, *callback.GetImpl());
}

void RejectConnection(connections::Core *pCore, const char *endpoint_id,
                      ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->RejectConnection(endpoint_id, *callback.GetImpl());
}

void SendPayload(connections::Core *pCore,
                 // todo(jfcarroll) this is being exported, needs to be
                 // refactored to return a plain old c type
                 const char **endpoint_ids, size_t endpoint_ids_size,
                 PayloadW payloadw, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  std::string payloadData = std::string(*endpoint_ids);
  absl::Span<const std::string> span{&payloadData, 1};
  pCore->SendPayload(span, std::move(*payloadw.GetImpl()), *callback.GetImpl());
}

void CancelPayload(connections::Core *pCore, std::int64_t payload_id,
                   ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->CancelPayload(payload_id, *callback.GetImpl());
}

void DisconnectFromEndpoint(connections::Core *pCore, const char *endpoint_id,
                            ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->DisconnectFromEndpoint(endpoint_id, *callback.GetImpl());
}

void StopAllEndpoints(connections::Core *pCore, ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->StopAllEndpoints(*callback.GetImpl());
}

void InitiateBandwidthUpgrade(connections::Core *pCore, char *endpoint_id,
                              ResultCallbackW callback) {
  if (pCore == nullptr) {
    return;
  }
  pCore->InitiateBandwidthUpgrade(endpoint_id, *callback.GetImpl());
}

const char *GetLocalEndpointId(connections::Core *pCore) {
  if (pCore == nullptr) {
    return "Null-Core";
  }
  std::string endpoint_id = pCore->GetLocalEndpointId();
  char *result = new char[endpoint_id.length() + 1];
  absl::SNPrintF(result, endpoint_id.length() + 1, "%s", endpoint_id);
  return result;
}

connections::ServiceControllerRouter *InitServiceControllerRouter() {
  return new connections::ServiceControllerRouter();
}

void CloseServiceControllerRouter(
    connections::ServiceControllerRouter *pRouter) {
  if (pRouter != nullptr) {
    delete pRouter;
  }
}

}  // namespace nearby::windows
