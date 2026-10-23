// Copyright 2026 Google LLC
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

#ifndef ANALYTICS_MOCK_ANALYTICS_RECORDER_H_
#define ANALYTICS_MOCK_ANALYTICS_RECORDER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/advertising_metadata_params.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/analytics/connection_attempt_metadata_params.h"
#include "connections/implementation/analytics/discovery_metadata_params.h"
#include "connections/payload_type.h"
#include "connections/strategy.h"
#include "internal/platform/error_code_params.h"
#include "proto/connections_enums.pb.h"

namespace nearby::analytics {

class MockAnalyticsRecorder : public AnalyticsRecorder {
 public:
  MockAnalyticsRecorder() = default;
  ~MockAnalyticsRecorder() override = default;

  // Advertising phase
  MOCK_METHOD(void, OnStartAdvertising,
              (connections::Strategy strategy,
               const std::vector<location::nearby::proto::connections::Medium>&
                   mediums,
               AdvertisingMetadataParams* advertising_metadata_params),
              (override));
  MOCK_METHOD(void, OnStopAdvertising, (), (override));

  MOCK_METHOD(int, GetNextAdvertisingUpdateIndex, (), (override));

  // Connection listening
  MOCK_METHOD(void, OnStartedIncomingConnectionListening,
              (connections::Strategy strategy), (override));
  MOCK_METHOD(void, OnStoppedIncomingConnectionListening, (), (override));

  // Discovery phase
  MOCK_METHOD(void, OnStartDiscovery,
              (connections::Strategy strategy,
               const std::vector<location::nearby::proto::connections::Medium>&
                   mediums,
               DiscoveryMetadataParams* discovery_metadata_params),
              (override));
  MOCK_METHOD(void, OnStopDiscovery, (), (override));

  MOCK_METHOD(int, GetNextDiscoveryUpdateIndex, (), (override));
  MOCK_METHOD(void, OnEndpointFound,
              (location::nearby::proto::connections::Medium medium),
              (override));

  // Connection request
  MOCK_METHOD(void, OnRequestConnection,
              (const connections::Strategy& strategy,
               const std::string& endpoint_id),
              (override));

  MOCK_METHOD(void, OnConnectionRequestReceived,
              (const std::string& remote_endpoint_id), (override));
  MOCK_METHOD(void, OnConnectionRequestSent,
              (const std::string& remote_endpoint_id), (override));
  MOCK_METHOD(void, OnRemoteEndpointAccepted,
              (const std::string& remote_endpoint_id), (override));
  MOCK_METHOD(void, OnLocalEndpointAccepted,
              (const std::string& remote_endpoint_id), (override));
  MOCK_METHOD(void, OnRemoteEndpointRejected,
              (const std::string& remote_endpoint_id), (override));
  MOCK_METHOD(void, OnLocalEndpointRejected,
              (const std::string& remote_endpoint_id), (override));

  // Connection attempt
  MOCK_METHOD(
      void, OnIncomingConnectionAttempt,
      (location::nearby::proto::connections::ConnectionAttemptType type,
       location::nearby::proto::connections::Medium medium,
       location::nearby::proto::connections::ConnectionAttemptResult result,
       absl::Duration duration, const std::string& connection_token,
       ConnectionAttemptMetadataParams* connection_attempt_metadata_params),
      (override));
  MOCK_METHOD(
      void, OnOutgoingConnectionAttempt,
      (const std::string& remote_endpoint_id,
       location::nearby::proto::connections::ConnectionAttemptType type,
       location::nearby::proto::connections::Medium medium,
       location::nearby::proto::connections::ConnectionAttemptResult result,
       absl::Duration duration, const std::string& connection_token,
       ConnectionAttemptMetadataParams* connection_attempt_metadata_params),
      (override));

  // Connection established
  MOCK_METHOD(void, OnConnectionEstablished,
              (const std::string& endpoint_id,
               location::nearby::proto::connections::Medium medium,
               const std::string& connection_token),
              (override));
  MOCK_METHOD(void, OnConnectionClosed,
              (const std::string& endpoint_id,
               location::nearby::proto::connections::Medium medium,
               location::nearby::proto::connections::DisconnectionReason reason,
               SafeDisconnectionResult result),
              (override));

  // Payload
  MOCK_METHOD(void, OnIncomingPayloadStarted,
              (const std::string& endpoint_id, std::int64_t payload_id,
               connections::PayloadType type, std::int64_t total_size_bytes),
              (override));
  MOCK_METHOD(void, OnPayloadChunkReceived,
              (const std::string& endpoint_id, std::int64_t payload_id,
               std::int64_t chunk_size_bytes),
              (override));
  MOCK_METHOD(void, OnIncomingPayloadDone,
              (const std::string& endpoint_id, std::int64_t payload_id,
               location::nearby::proto::connections::PayloadStatus status,
               location::nearby::proto::connections::OperationResultCode
                   operation_result_code),
              (override));
  MOCK_METHOD(void, OnOutgoingPayloadStarted,
              (const std::vector<std::string>& endpoint_ids,
               std::int64_t payload_id, connections::PayloadType type,
               std::int64_t total_size_bytes),
              (override));
  MOCK_METHOD(void, OnPayloadChunkSent,
              (const std::string& endpoint_id, std::int64_t payload_id,
               std::int64_t chunk_size_bytes),
              (override));
  MOCK_METHOD(void, OnOutgoingPayloadDone,
              (const std::string& endpoint_id, std::int64_t payload_id,
               location::nearby::proto::connections::PayloadStatus status,
               location::nearby::proto::connections::OperationResultCode
                   operation_result_code),
              (override));

  // BandwidthUpgrade
  MOCK_METHOD(void, OnBandwidthUpgradeStarted,
              (const std::string& endpoint_id,
               location::nearby::proto::connections::Medium from_medium,
               location::nearby::proto::connections::Medium to_medium,
               location::nearby::proto::connections::ConnectionAttemptDirection
                   direction,
               const std::string& connection_token),
              (override));
  MOCK_METHOD(void, UpdateBwUpgradeNetworkInfo,
              (const std::string& endpoint_id, int num_interfaces,
               int num_ipv6_only_interfaces),
              (override));
  MOCK_METHOD(void, OnBandwidthUpgradeError,
              (const std::string& endpoint_id,
               location::nearby::proto::connections::BandwidthUpgradeResult
                   result,
               location::nearby::proto::connections::BandwidthUpgradeErrorStage
                   error_stage,
               location::nearby::proto::connections::OperationResultCode
                   operation_result_code),
              (override));
  MOCK_METHOD(void, OnBandwidthUpgradeSuccess, (const std::string& endpoint_id),
              (override));

  // Error Code
  MOCK_METHOD(void, OnErrorCode, (const ErrorCodeParams& params), (override));

  MOCK_METHOD(void, LogStartSession, (), (override));
  MOCK_METHOD(void, LogSession, (), (override));

  MOCK_METHOD(bool, IsSessionLogged, (), (override));

  MOCK_METHOD(
      location::nearby::proto::connections::OperationResultCategory,
      GetOperationResultCategory,
      (location::nearby::proto::connections::OperationResultCode result_code),
      (override));

  MOCK_METHOD(void, Sync, (), (override));
};

}  // namespace nearby::analytics

#endif  // ANALYTICS_MOCK_ANALYTICS_RECORDER_H_
