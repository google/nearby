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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_ANALYTICS_ANALYTICS_RECORDER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_ANALYTICS_ANALYTICS_RECORDER_H_

#include <memory>

#include "internal/analytics/event_logger.h"
#include "internal/proto/analytics/fast_pair_log.proto.h"
#include "proto/fast_pair_enums.proto.h"

namespace nearby {
namespace fastpair {
namespace analytics {

class AnalyticsRecorder {
 public:
  explicit AnalyticsRecorder(::nearby::analytics::EventLogger* event_logger)
      : event_logger_(event_logger) {}
  ~AnalyticsRecorder() = default;

  void NewGattEvent(int error_from_os);

  void NewBrEdrHandoverEvent(
      nearby::proto::fastpair::FastPairEvent::BrEdrHandoverErrorCode error_code

  );

  void NewCreateBondEvent(
      ::nearby::proto::fastpair::FastPairEvent::CreateBondErrorCode error_code,
      int unbond_reason);

  void NewConnectEvent(
      nearby::proto::fastpair::FastPairEvent::ConnectErrorCode error_code,
      int profile_uuid);

  void NewProviderInfo(int number_account_keys_on_provider);

  void NewFootprintsInfo(int number_devices_on_footprints);

  void NewKeyBasedPairingInfo(int request_flag, int response_type,
                              int response_flag, int response_device_count);

 private:
  void LogEvent(const nearby::proto::fastpair::FastPairLog& message);

  nearby::analytics::EventLogger* event_logger_ = nullptr;
};

}  // namespace analytics
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_ANALYTICS_ANALYTICS_RECORDER_H_
