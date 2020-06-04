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

#ifndef CORE_V2_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_
#define CORE_V2_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_

#include <cstdint>
#include <vector>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/system_clock.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Representation of a GATT advertisement read result. This object helps us
// determine whether or not we need to retry GATT reads.
class AdvertisementReadResult {
 public:
  // We need a long enough duration such that we always trigger a read
  // retry AND we always connect to it without delay. The former case
  // helps us initialize an AdvertisementReadResult so that we
  // unconditionally try reading on the first sighting. And the latter
  // case helps us connect immediately when we initialize a dummy read
  // result for fast advertisements (which don't use the GATT server).

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
  ~AdvertisementReadResult() = default;

  enum class RetryStatus {
    kUnknown = 0,
    kRetry = 1,
    kPreviouslySucceeded = 2,
    kTooSoon = 3,
  };

  void AddAdvertisement(std::int32_t slot, const ByteArray& advertisement)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool HasAdvertisement(std::int32_t slot) const ABSL_LOCKS_EXCLUDED(mutex_);
  std::vector<const ByteArray*> GetAdvertisements() const
      ABSL_LOCKS_EXCLUDED(mutex_);
  RetryStatus EvaluateRetryStatus() const ABSL_LOCKS_EXCLUDED(mutex_);
  void RecordLastReadStatus(bool is_success) ABSL_LOCKS_EXCLUDED(mutex_);
  absl::Duration GetDurationSinceRead() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  enum class Status {
    kUnknown = 0,
    kSuccess = 1,
    kFailure = 2,
  };

  absl::Duration GetDurationSinceReadLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;

  // Maps slot numbers to the GATT advertisement found in that slot.
  absl::flat_hash_map<std::int32_t, ByteArray> advertisements_
      ABSL_GUARDED_BY(mutex_);

  Config config_;
  absl::Duration backoff_duration_ ABSL_GUARDED_BY(mutex_);
  absl::Time last_read_timestamp_ ABSL_GUARDED_BY(mutex_);
  Status status_ ABSL_GUARDED_BY(mutex_) = Status::kUnknown;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_
