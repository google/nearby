// Copyright 2020 Google LLC
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

#ifndef PLATFORM_BASE_FEATURE_FLAGS_H_
#define PLATFORM_BASE_FEATURE_FLAGS_H_

#include <cstdint>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace nearby {

// Global flags that control feature availability. This may be used to gating
// features in development, QA testing and releasing.
class FeatureFlags {
 public:
  // Holds for all the feature flags.
  struct Flags {
    bool enable_cancellation_flag = true;
    bool enable_async_bandwidth_upgrade = true;
    // If a scheduled runnable is already running, Cancel() will synchronously
    // wait for the task to complete.
    bool cancel_waits_for_running_tasks = true;
    // Keep Alive frame interval and timeout in millis.
    std::int32_t keep_alive_interval_millis = 5000;
    std::int32_t keep_alive_timeout_millis = 30000;
    bool use_exp_backoff_in_bwu_retry = true;
    // without the exp backoff, retry intervals in seconds: 5, 10, 10, 10...
    // with the exp backoff, retry intervals in seconds: 3, 6, 12, 24...
    absl::Duration bwu_retry_exp_backoff_initial_delay = absl::Seconds(3);
    absl::Duration bwu_retry_exp_backoff_maximum_delay = absl::Seconds(300);
    // Support sending file and stream payloads starting from a non-zero offset.
    bool enable_send_payload_offset = true;
    // Provide better bookkeeping for bandwidth upgrade initiation. This is
    // necessary to properly support multiple BWU mediums, multiple service, and
    // multiple endpionts.
    bool support_multiple_bwu_mediums = true;
    // Allows the code to change the bluetooth radio state
    bool enable_set_radio_state = false;
    // If the feature is enabled, medium connection will timeout when cannot
    // create connection with remote device in a duration.
    bool enable_connection_timeout = true;
    // Controls enable or disable to track the status of Bluetooth classic
    // conncetion.
    bool enable_bluetooth_connection_status_track = true;
    // Controls enable or disable BLE scan advertisement for fast pair
    // service uuid 0x2cfe
    bool enable_scan_for_fast_pair_advertisement = false;
    // Skip Service Discovery Protocol check if the remote party supports the
    // requested service id before attempting to connect over rfcomm. SDP fails
    // on Windows when connecting to FP service id but the rfcomm is successful.
    bool skip_service_discovery_before_connecting_to_rfcomm = false;
    std::int32_t min_nc_version_supports_safe_to_disconnect = 1;
    // Android code won't be able to launch "payload_received_ack" feature for
    // in near future, so change "payload_received_ack" version from "2" to "5"
    // after auto-reconnect and auto-resume.
    std::int32_t min_nc_version_supports_payload_received_ack = 5;
    // If the other part doesn't ack the safe_to_disconnect request, the
    // initiator will end the connection in 30s.
    absl::Duration safe_to_disconnect_ack_delay_millis =
        absl::Milliseconds(30000);
    absl::Duration safe_to_disconnect_remote_disc_delay_millis =
        absl::Milliseconds(10000);
    // If the receiver doesn't ack with payload_received_ack frame in 1s, the
    // sender will timeout the waiting.
    absl::Duration wait_payload_received_ack_millis = absl::Milliseconds(1000);
  };

  static const FeatureFlags& GetInstance() {
    static const FeatureFlags* instance = new FeatureFlags();
    return *instance;
  }

  const Flags& GetFlags() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::ReaderMutexLock lock(&mutex_);
    return flags_;
  }

  static Flags& GetMutableFlagsForTesting() {
    return const_cast<FeatureFlags&>(GetInstance()).flags_;
  }

  // SetFlags for feature controlling
  void SetFlags(const Flags& flags) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    flags_ = flags;
  }

 private:
  FeatureFlags() = default;
  Flags flags_ ABSL_GUARDED_BY(mutex_);
  mutable absl::Mutex mutex_;
};
}  // namespace nearby

#endif  // PLATFORM_BASE_FEATURE_FLAGS_H_
