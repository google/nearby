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

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace nearby {

// Global flags that control feature availability. This may be used to gating
// features in development, QA testing and releasing.
class FeatureFlags {
 public:
  // Holds for all the feature flags.
  struct Flags {
    bool enable_cancellation_flag = false;
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
    // Ble v2/v1 switch flag: the flag will be removed once v2 refactor is done.
    bool support_ble_v2 = false;
    // Allows the code to change the bluetooth radio state
    bool enable_set_radio_state = false;
    // If the feature is enabled, medium connection will timeout when cannot
    // create connection with remote device in a duration.
    bool enable_connection_timeout = true;
    // Controls to enable or disable to track the status of Bluetooth classic
    // conncetion.
    bool enable_bluetooth_connection_status_track = true;
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

 private:
  FeatureFlags() = default;

  // MediumEnvironment is testing util class. Use friend class here to enable
  // SetFlags for feature controlling need in test environment.
  friend class MediumEnvironment;
  void SetFlags(const Flags& flags) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    flags_ = flags;
  }
  Flags flags_ ABSL_GUARDED_BY(mutex_);
  mutable absl::Mutex mutex_;
};
}  // namespace nearby

#endif  // PLATFORM_BASE_FEATURE_FLAGS_H_
