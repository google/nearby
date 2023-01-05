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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_ADVERTISEMENT_READ_RESULT_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_ADVERTISEMENT_READ_RESULT_H_

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/clock.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace connections {
namespace mediums {

// Representation of a GATT/L2CAP advertisement read result. This object helps
// us determine whether or not we need to retry GATT reads.
class AdvertisementReadResult {
 public:
  struct Config {
    // How much to multiply the backoff duration by with every failure to read
    // from the advertisement GATT server. This should never be below 1!
    float backoff_multiplier;
    // The initial backoff duration when we fail to read from an advertisement
    // GATT server.
    absl::Duration base_backoff_duration;
    // The maximum backoff duration allowed between advertisement GATT server
    // reads.
    absl::Duration max_backoff_duration;
  };

  static const Config kDefaultConfig;

  explicit AdvertisementReadResult(const Config& config = kDefaultConfig)
      : config_(config) {}

  // Indicator for deciding if we should retry reading the advertisement.
  enum class RetryStatus {
    kUnknown = 0,
    kRetry = 1,
    kPreviouslySucceeded = 2,
    kTooSoon = 3,
  };

  // Adds a successfully read advertisement for the specified slot to this read
  // result. This is fundamentally different from  RecordLastReadStatus()
  // because we can report a read failure, but still manage to read some
  // advertisements.
  void AddAdvertisement(int slot, const ByteArray& advertisement)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Determines whether or not an advertisement was successfully read at the
  // specified slot.
  bool HasAdvertisement(int slot) const ABSL_LOCKS_EXCLUDED(mutex_);

  // Retrieves all raw advertisements that were successfully read.
  std::vector<const ByteArray*> GetAdvertisements() const
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Determines what stage we're in for retrying a read from an advertisement
  // GATT/L2CAP server.
  RetryStatus EvaluateRetryStatus() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Records the status of the latest read, and updates the next backoff
  // duration for subsequent reads. Be sure to also call AddAdvertisement(...)
  // if any advertisements were read.
  void RecordLastReadStatus(bool is_success) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns how much time has passed since we last tried reading from an
  // advertisement GATT server.
  absl::Duration GetDurationSinceRead() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  enum class Result {
    kFailure = 0,
    kSuccess = 1,
  };

  absl::Duration GetDurationSinceReadLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;

  // Maps slot numbers to the GATT advertisement found in that slot.
  absl::flat_hash_map<int, ByteArray> advertisements_ ABSL_GUARDED_BY(mutex_);

  Config config_;
  absl::Duration backoff_duration_ ABSL_GUARDED_BY(mutex_) =
      config_.base_backoff_duration;

  // We need a long enough duration such that we always trigger a read retry AND
  // we always connect to it without delay. The former case helps us initialize
  // an AdvertisementReadResult so that we unconditionally try reading on the
  // first sighting. And the latter case helps us connect immediately when we
  // initialize a dummy read result for fast advertisements (which don't use the
  // GATT server).
  absl::Time last_read_timestamp_ ABSL_GUARDED_BY(mutex_) =
      SystemClock::ElapsedRealtime() - config_.max_backoff_duration;
  absl::optional<Result> result_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_ADVERTISEMENT_READ_RESULT_H_
