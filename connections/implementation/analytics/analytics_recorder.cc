// Copyright 2022-2023 Google LLC
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

#include "connections/implementation/analytics/analytics_recorder.h"

#include <memory>
#include <string>
#include <vector>

#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/implementation/analytics/operation_result_with_medium.h"
#include "proto/connections_enums.pb.h"

namespace nearby::analytics {

using ::location::nearby::proto::connections::ConnectionBand;
using ::location::nearby::proto::connections::ConnectionTechnology;
using ::location::nearby::proto::connections::Medium;
using ::location::nearby::proto::connections::OperationResultCode;

std::unique_ptr<AdvertisingMetadataParams>
AnalyticsRecorder::BuildAdvertisingMetadataParams(
    bool is_extended_advertisement_supported, int connected_ap_frequency,
    bool is_nfc_available,
    const std::vector<OperationResultWithMedium>&
        operation_result_with_mediums) {
  auto params = std::make_unique<AdvertisingMetadataParams>();
  params->is_extended_advertisement_supported =
      is_extended_advertisement_supported;
  params->connected_ap_frequency = connected_ap_frequency;
  params->is_nfc_available = is_nfc_available;
  params->operation_result_with_mediums = operation_result_with_mediums;
  return params;
}

std::unique_ptr<DiscoveryMetadataParams>
AnalyticsRecorder::BuildDiscoveryMetadataParams(
    bool is_extended_advertisement_supported, int connected_ap_frequency,
    bool is_nfc_available,
    const std::vector<OperationResultWithMedium>&
        operation_result_with_mediums) {
  auto params = std::make_unique<DiscoveryMetadataParams>();
  params->is_extended_advertisement_supported =
      is_extended_advertisement_supported;
  params->connected_ap_frequency = connected_ap_frequency;
  params->is_nfc_available = is_nfc_available;
  params->operation_result_with_mediums = operation_result_with_mediums;
  return params;
}

std::unique_ptr<ConnectionAttemptMetadataParams>
AnalyticsRecorder::BuildConnectionAttemptMetadataParams(
    ConnectionTechnology technology, ConnectionBand band, int frequency,
    int try_count, const std::string& network_operator,
    const std::string& country_code, bool is_tdls_used,
    bool wifi_hotspot_enabled, int max_wifi_tx_speed, int max_wifi_rx_speed,
    int channel_width, OperationResultCode operation_result_code) {
  auto params = std::make_unique<ConnectionAttemptMetadataParams>();
  params->technology = technology;
  params->band = band;
  params->frequency = frequency;
  params->try_count = try_count;
  params->network_operator = network_operator;
  params->country_code = country_code;
  params->is_tdls_used = is_tdls_used;
  params->wifi_hotspot_enabled = wifi_hotspot_enabled;
  params->max_wifi_tx_speed = max_wifi_tx_speed;
  params->max_wifi_rx_speed = max_wifi_rx_speed;
  params->channel_width = channel_width;
  params->operation_result_code = operation_result_code;
  return params;
}

OperationResultCode AnalyticsRecorder::GetChannelIoErrorResultCodeFromMedium(
    Medium medium) {
  switch (medium) {
    case Medium::BLUETOOTH:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BT;
    case Medium::WIFI_HOTSPOT:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_HOTSPOT;
    case Medium::BLE:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BLE;
    case Medium::BLE_L2CAP:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_BLE_L2CAP;
    case Medium::WIFI_LAN:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_LAN;
    case Medium::WIFI_AWARE:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_AWARE;
    case Medium::NFC:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_NFC;
    case Medium::WIFI_DIRECT:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WIFI_DIRECT;
    case Medium::WEB_RTC:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_WEB_RTC;
    case Medium::AWDL:
      return OperationResultCode::CONNECTIVITY_CHANNEL_IO_ERROR_ON_AWDL;
    default:
      return OperationResultCode::
          CONNECTIVITY_CHANNEL_IO_ERROR_ON_UNKNOWN_MEDIUM;
  }
}

}  // namespace nearby::analytics
