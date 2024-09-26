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

#include "absl/base/thread_annotations.h"
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
    // multiple endpoints.
    bool support_multiple_bwu_mediums = true;
    // Allows the code to change the bluetooth radio state
    bool enable_set_radio_state = false;
    // If the feature is enabled, medium connection will timeout when cannot
    // create connection with remote device in a duration.
    bool enable_connection_timeout = true;
    // Controls enable or disable to track the status of Bluetooth classic
    // connection.
    bool enable_bluetooth_connection_status_track = true;
    // Controls enable or disable BLE scan advertisement for fast pair
    // service uuid 0x2cfe
    bool enable_scan_for_fast_pair_advertisement = false;
    // Skip Service Discovery Protocol check if the remote party supports the
    // requested service id before attempting to connect over rfcomm. SDP fails
    // on Windows when connecting to FP service id but the rfcomm is successful.
    bool skip_service_discovery_before_connecting_to_rfcomm = false;
    // Controls enable or disable the use of async methods for StartScanning
    // and StopScanning for BLE V2.
    // TODO(b/333408829): Add flag to control async advertising.
    bool enable_ble_v2_async_scanning = false;
    // Enable legacy device discovered callback being used inside ble v2
    // DiscoverPeripheralTracker flow.
    bool enable_invoking_legacy_device_discovered_cb = false;

    // Enable 1. safe-to-disconnect check 2. reserved 3. auto-reconnect 4.
    // auto-resume 5. non-distance-constraint-recovery 6. payload_ack
    std::int32_t min_nc_version_supports_safe_to_disconnect = 1;
    std::int32_t min_nc_version_supports_auto_reconnect = 3;
    absl::Duration auto_reconnect_retry_delay_millis = absl::Milliseconds(5000);
    absl::Duration auto_reconnect_timeout_millis = absl::Milliseconds(30000);
    std::int32_t auto_reconnect_retry_attempts = 3;
    absl::Duration auto_reconnect_skip_duplicated_endpoint_duration =
        absl::Milliseconds(4000);
    // Android code won't be able to launch "payload_received_ack" feature for
    // in near future, so change "payload_received_ack" version from "2" to "5"
    // after auto-reconnect and auto-resume.
    std::int32_t min_nc_version_supports_payload_received_ack = 6;
    // If the other part doesn't ack the safe_to_disconnect request, the
    // initiator will end the connection in 30s.
    absl::Duration safe_to_disconnect_ack_delay_millis =
        absl::Milliseconds(30000);
    absl::Duration safe_to_disconnect_remote_disc_delay_millis =
        absl::Milliseconds(10000);
    absl::Duration safe_to_disconnect_auto_resume_timeout_millis =
        absl::Milliseconds(60000);
    // If the receiver doesn't ack with payload_received_ack frame in 1s, the
    // sender will timeout the waiting.
    absl::Duration wait_payload_received_ack_millis = absl::Milliseconds(1000);

    // Multiplex related flags
    // Timeout value for read frame operation in endpoint channel.
    absl::Duration mediums_frame_read_timeout_millis =
        absl::Milliseconds(15000);
    // Timeout value for write frame operation in endpoint channel.
    absl::Duration mediums_frame_write_timeout_millis =
        absl::Milliseconds(15000);
    // The timeout for waiting on connection request response.
    absl::Duration multiplex_socket_connection_response_timeout_millis =
        absl::Milliseconds(3000);
    // The capacity of the middle priority queue inner MultiplexOutputStream.
    // The new outgoing frame with the middle priority will wait for space to
    // become available if the queue is full.'
    std::uint32_t multiplex_socket_middle_priority_queue_capacity = 50;
    // The maximum size of frame we'll attempt to read, to avoid a remote device
    // from triggering an OutOfMemory error.
    std::uint32_t connection_max_frame_length = 1048576;
    std::uint32_t blocking_queue_stream_queue_capacity = 10;
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
