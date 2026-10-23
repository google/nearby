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

#ifndef ANALYTICS_ANALYTICS_RECORDER_H_
#define ANALYTICS_ANALYTICS_RECORDER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/implementation/analytics/operation_result_with_medium.h"
#include "connections/payload_type.h"
#include "connections/strategy.h"
#include "internal/platform/error_code_params.h"
#include "proto/connections_enums.pb.h"

namespace nearby::analytics {

enum class SafeDisconnectionResult {
  kUnknown = 0,
  kSafeDisconnection = 1,
  kUnsafeDisconnection = 2,
};

class AnalyticsRecorder {
 public:
  AnalyticsRecorder() = default;
  virtual ~AnalyticsRecorder() = default;

  // Advertising phase
  virtual void OnStartAdvertising(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium>& mediums,
      AdvertisingMetadataParams* advertising_metadata_params) = 0;
  virtual void OnStopAdvertising() = 0;

  virtual int GetNextAdvertisingUpdateIndex() = 0;

  // Connection listening
  virtual void OnStartedIncomingConnectionListening(
      connections::Strategy strategy) = 0;
  virtual void OnStoppedIncomingConnectionListening() = 0;

  // Discovery phase
  virtual void OnStartDiscovery(
      connections::Strategy strategy,
      const std::vector<location::nearby::proto::connections::Medium>& mediums,
      DiscoveryMetadataParams* discovery_metadata_params) = 0;
  virtual void OnStopDiscovery() = 0;

  virtual int GetNextDiscoveryUpdateIndex() = 0;
  virtual void OnEndpointFound(
      location::nearby::proto::connections::Medium medium) = 0;

  // Connection request
  virtual void OnRequestConnection(const connections::Strategy& strategy,
                                   const std::string& endpoint_id) = 0;

  virtual void OnConnectionRequestReceived(
      const std::string& remote_endpoint_id) = 0;
  virtual void OnConnectionRequestSent(
      const std::string& remote_endpoint_id) = 0;
  virtual void OnRemoteEndpointAccepted(
      const std::string& remote_endpoint_id) = 0;
  virtual void OnLocalEndpointAccepted(
      const std::string& remote_endpoint_id) = 0;
  virtual void OnRemoteEndpointRejected(
      const std::string& remote_endpoint_id) = 0;
  virtual void OnLocalEndpointRejected(
      const std::string& remote_endpoint_id) = 0;

  // Connection attempt
  virtual void OnIncomingConnectionAttempt(
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string& connection_token,
      ConnectionAttemptMetadataParams* connection_attempt_metadata_params) = 0;
  virtual void OnOutgoingConnectionAttempt(
      const std::string& remote_endpoint_id,
      location::nearby::proto::connections::ConnectionAttemptType type,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::ConnectionAttemptResult result,
      absl::Duration duration, const std::string& connection_token,
      ConnectionAttemptMetadataParams* connection_attempt_metadata_params) = 0;

  static std::unique_ptr<AdvertisingMetadataParams>
  BuildAdvertisingMetadataParams(
      bool is_extended_advertisement_supported = false,
      int connected_ap_frequency = 0, bool is_nfc_available = false,
      const std::vector<OperationResultWithMedium>&
          operation_result_with_mediums = {});
  static std::unique_ptr<DiscoveryMetadataParams> BuildDiscoveryMetadataParams(
      bool is_extended_advertisement_supported = false,
      int connected_ap_frequency = 0, bool is_nfc_available = false,
      const std::vector<OperationResultWithMedium>&
          operation_result_with_mediums = {});

  static std::unique_ptr<ConnectionAttemptMetadataParams>
  BuildConnectionAttemptMetadataParams(
      location::nearby::proto::connections::ConnectionTechnology technology,
      location::nearby::proto::connections::ConnectionBand band, int frequency,
      int try_count, const std::string& network_operator = {},
      const std::string& country_code = {}, bool is_tdls_used = false,
      bool wifi_hotspot_enabled = false, int max_wifi_tx_speed = 0,
      int max_wifi_rx_speed = 0, int channel_width = -1,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code = location::nearby::proto::connections::
              OperationResultCode::DETAIL_UNKNOWN);
  static location::nearby::proto::connections::OperationResultCode
  GetChannelIoErrorResultCodeFromMedium(
      location::nearby::proto::connections::Medium medium);

  // Connection established
  virtual void OnConnectionEstablished(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium medium,
      const std::string& connection_token) = 0;
  virtual void OnConnectionClosed(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium medium,
      location::nearby::proto::connections::DisconnectionReason reason,
      SafeDisconnectionResult result) = 0;

  // Payload
  virtual void OnIncomingPayloadStarted(const std::string& endpoint_id,
                                        std::int64_t payload_id,
                                        connections::PayloadType type,
                                        std::int64_t total_size_bytes) = 0;
  virtual void OnPayloadChunkReceived(const std::string& endpoint_id,
                                      std::int64_t payload_id,
                                      std::int64_t chunk_size_bytes) = 0;
  virtual void OnIncomingPayloadDone(
      const std::string& endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) = 0;
  virtual void OnOutgoingPayloadStarted(
      const std::vector<std::string>& endpoint_ids, std::int64_t payload_id,
      connections::PayloadType type, std::int64_t total_size_bytes) = 0;
  virtual void OnPayloadChunkSent(const std::string& endpoint_id,
                                  std::int64_t payload_id,
                                  std::int64_t chunk_size_bytes) = 0;
  virtual void OnOutgoingPayloadDone(
      const std::string& endpoint_id, std::int64_t payload_id,
      location::nearby::proto::connections::PayloadStatus status,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) = 0;

  // BandwidthUpgrade
  virtual void OnBandwidthUpgradeStarted(
      const std::string& endpoint_id,
      location::nearby::proto::connections::Medium from_medium,
      location::nearby::proto::connections::Medium to_medium,
      location::nearby::proto::connections::ConnectionAttemptDirection
          direction,
      const std::string& connection_token) = 0;
  virtual void UpdateBwUpgradeNetworkInfo(const std::string& endpoint_id,
                                          int num_interfaces,
                                          int num_ipv6_only_interfaces) = 0;
  virtual void OnBandwidthUpgradeError(
      const std::string& endpoint_id,
      location::nearby::proto::connections::BandwidthUpgradeResult result,
      location::nearby::proto::connections::BandwidthUpgradeErrorStage
          error_stage,
      location::nearby::proto::connections::OperationResultCode
          operation_result_code) = 0;
  virtual void OnBandwidthUpgradeSuccess(const std::string& endpoint_id) = 0;

  // Error Code
  virtual void OnErrorCode(const ErrorCodeParams& params) = 0;

  virtual void LogStartSession() = 0;
  virtual void LogSession() = 0;

  virtual bool IsSessionLogged() = 0;

  virtual location::nearby::proto::connections::OperationResultCategory
  GetOperationResultCategory(
      location::nearby::proto::connections::OperationResultCode
          result_code) = 0;

  virtual void Sync() = 0;
};

}  // namespace nearby::analytics

#endif  // ANALYTICS_ANALYTICS_RECORDER_H_
