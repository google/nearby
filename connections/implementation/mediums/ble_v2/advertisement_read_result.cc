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

#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"

#include <algorithm>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/time/clock.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {
namespace mediums {

const AdvertisementReadResult::Config AdvertisementReadResult::kDefaultConfig{
    .backoff_multiplier = 2.0,
    .base_backoff_duration = absl::Seconds(1),
    .max_backoff_duration = absl::Minutes(5),
};

void AdvertisementReadResult::AddAdvertisement(int slot,
                                               const ByteArray& advertisement) {
  MutexLock lock(&mutex_);

  // Replace if existed.
  advertisements_.insert_or_assign(slot, advertisement);
}

bool AdvertisementReadResult::HasAdvertisement(int slot) const {
  MutexLock lock(&mutex_);

  return advertisements_.contains(slot);
}

std::vector<const ByteArray*> AdvertisementReadResult::GetAdvertisements()
    const {
  MutexLock lock(&mutex_);

  std::vector<const ByteArray*> all_advertisements;
  std::transform(advertisements_.begin(), advertisements_.end(),
                 std::back_inserter(all_advertisements),
                 [](auto& kv) { return &kv.second; });

  return all_advertisements;
}

// Determines what stage we're in for retrying a read from an advertisement
// GATT server.
AdvertisementReadResult::RetryStatus
AdvertisementReadResult::EvaluateRetryStatus() const {
  MutexLock lock(&mutex_);

  // Check if we have already succeeded reading this advertisement.
  if (result_.has_value() && *result_ == Result::kSuccess) {
    return RetryStatus::kPreviouslySucceeded;
  }

  // Check if we have recently failed to read this advertisement.
  if (GetDurationSinceReadLocked() < backoff_duration_) {
    return RetryStatus::kTooSoon;
  }

  return RetryStatus::kRetry;
}

void AdvertisementReadResult::RecordLastReadStatus(bool is_success) {
  MutexLock lock(&mutex_);

  // Update the last read timestamp.
  last_read_timestamp_ = SystemClock::ElapsedRealtime();

  // Update the backoff duration.
  if (is_success) {
    // Reset the backoff duration now that we had a successful read.
    backoff_duration_ = config_.base_backoff_duration;
  } else {
    // Determine whether or not we were already failing before. If we were, we
    // should increase the backoff duration.
    if (result_.has_value() && *result_ == Result::kFailure) {
      // Use exponential backoff to determine the next backoff duration. This
      // simply involves multiplying our current backoff duration by some
      // multiplier.
      absl::Duration next_backoff_duration =
          config_.backoff_multiplier * backoff_duration_;
      // Update the backoff duration, making sure not to blow past the
      // ceiling.
      backoff_duration_ =
          std::min(next_backoff_duration, config_.max_backoff_duration);
    } else {
      // This is our first time failing, so we should only backoff for the
      // initial duration.
      backoff_duration_ = config_.base_backoff_duration;
    }
  }

  // Update the internal result.
  result_ = is_success ? Result::kSuccess : Result::kFailure;
}

absl::Duration AdvertisementReadResult::GetDurationSinceRead() const {
  MutexLock lock(&mutex_);
  return GetDurationSinceReadLocked();
}

absl::Duration AdvertisementReadResult::GetDurationSinceReadLocked() const {
  return SystemClock::ElapsedRealtime() - last_read_timestamp_;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
