// Copyright 2023 Google LLC
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

#include "fastpair/analytics/analytics_recorder.h"

#include <memory>

#include "internal/analytics/event_logger.h"
#include "internal/proto/analytics/fast_pair_log.proto.h"
#include "proto/fast_pair_enums.proto.h"

namespace nearby {
namespace fastpair {
namespace analytics {

using ::nearby::fastpair::analytics::AnalyticsRecorder;
using ::nearby::proto::fastpair::FastPairLog;

void AnalyticsRecorder::NewGattEvent(int error_from_os) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto gatt_event = FastPairLog::GattEvent::default_instance().New();
  gatt_event->set_error_from_os(error_from_os);

  fast_pair_log->set_allocated_gatt_event(gatt_event);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewBrEdrHandoverEvent(
    ::nearby::proto::fastpair::FastPairEvent::BrEdrHandoverErrorCode
        error_code) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto br_edr_event = FastPairLog::BrEdrHandoverEvent::default_instance().New();
  br_edr_event->set_error_code(error_code);

  fast_pair_log->set_allocated_br_edr_handover_event(br_edr_event);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewCreateBondEvent(
    ::nearby::proto::fastpair::FastPairEvent::CreateBondErrorCode error_code,
    int unbond_reason) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto create_bond_event =
      FastPairLog::CreateBondEvent::default_instance().New();
  create_bond_event->set_error_code(error_code);
  create_bond_event->set_unbond_reason(unbond_reason);

  fast_pair_log->set_allocated_bond_event(create_bond_event);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewConnectEvent(
    ::nearby::proto::fastpair::FastPairEvent::ConnectErrorCode error_code,
    int profile_uuid) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto connect_event = FastPairLog::ConnectEvent::default_instance().New();
  connect_event->set_error_code(error_code);
  connect_event->set_profile_uuid(profile_uuid);

  fast_pair_log->set_allocated_connect_event(connect_event);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewProviderInfo(int number_account_keys_on_provider) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto provider_info = FastPairLog::ProviderInfo::default_instance().New();
  provider_info->set_number_account_keys_on_provider(
      number_account_keys_on_provider);

  fast_pair_log->set_allocated_provider_info(provider_info);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewFootprintsInfo(int number_devices_on_footprints) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto footprints_info = FastPairLog::FootprintsInfo::default_instance().New();
  footprints_info->set_number_devices_on_footprints(
      number_devices_on_footprints);

  fast_pair_log->set_allocated_footprints_info(footprints_info);
  LogEvent(*fast_pair_log);
}

void AnalyticsRecorder::NewKeyBasedPairingInfo(int request_flag,
                                               int response_type,
                                               int response_flag,
                                               int response_device_count) {
  auto fast_pair_log = std::make_unique<FastPairLog>();

  auto key_based_pairing_info =
      FastPairLog::KeyBasedPairingInfo::default_instance().New();
  key_based_pairing_info->set_request_flag(request_flag);
  key_based_pairing_info->set_response_flag(response_flag);
  key_based_pairing_info->set_response_device_count(response_device_count);
  key_based_pairing_info->set_response_type(response_type);

  fast_pair_log->set_allocated_key_based_pairing_info(key_based_pairing_info);
  LogEvent(*fast_pair_log);
}

// start private methods

void AnalyticsRecorder::LogEvent(const FastPairLog& message) {
  if (event_logger_ == nullptr) {
    return;
  }
  event_logger_->Log(message);
}

}  // namespace analytics
}  // namespace fastpair
}  // namespace nearby
