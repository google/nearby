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

#include "core/internal/mediums/advertisement_read_result.h"

#include <algorithm>

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

template <typename K, typename V>
void eraseOwnedPtrFromMap(std::map<K, ConstPtr<V> >& m, const K& k) {
  typename std::map<K, ConstPtr<V> >::iterator it = m.find(k);
  if (it != m.end()) {
    it->second.destroy();
    m.erase(it);
  }
}

}  // namespace

// How much to multiply the backoff duration by with every failure to read
// from the advertisement GATT server. This should never be below 1!
template <typename Platform>
const float AdvertisementReadResult<Platform>::kAdvertisementBackoffMultiplier =
    2.0;

// The initial backoff duration when we fail to read from an advertisement
// GATT server.
template <typename Platform>
const std::int64_t
    AdvertisementReadResult<Platform>::kAdvertisementBaseBackoffDurationMillis =
        1 * 1000;  // 1 second

// The maximum backoff duration allowed between advertisement GATT server
// reads.
template <typename Platform>
const std::int64_t
    AdvertisementReadResult<Platform>::kAdvertisementMaxBackoffDurationMillis =
        5 * 60 * 1000;  // 5 minutes

template <typename Platform>
AdvertisementReadResult<Platform>::AdvertisementReadResult()
    : lock_(Platform::createLock()),
      system_clock_(Platform::createSystemClock()),
      advertisements_(),
      backoff_duration_millis_(kAdvertisementBaseBackoffDurationMillis),
      // We need a long enough duration such that we always trigger a read
      // retry AND we always connect to it without delay. The former case
      // helps us initialize an AdvertisementReadResult so that we
      // unconditionally try reading on the first sighting. And the latter
      // case helps us connect immediately when we initialize a dummy read
      // result for fast advertisements (which don't use the GATT server).
      last_read_timestamp_millis_(system_clock_->elapsedRealtime() -
                                  kAdvertisementMaxBackoffDurationMillis),
      result_(Result::Value::UNKNOWN) {}

template <typename Platform>
AdvertisementReadResult<Platform>::~AdvertisementReadResult() {
  Synchronized s(lock_.get());

  for (AdvertisementMap::iterator it = advertisements_.begin();
       it != advertisements_.end(); ++it) {
    it->second.destroy();
  }
  advertisements_.clear();
}

// Adds a successfully read advertisement for the specified slot to this read
// result. This is fundamentally different from
// {@link #recordLastReadStatus(boolean)} because we can report a read
// failure, but still manage to read some advertisements.
// Note: advertisement should be passed in as a RefCounted Ptr. It is not the
// responsibility of AdvertisementReadResult to make it RefCounted.
template <typename Platform>
void AdvertisementReadResult<Platform>::addAdvertisement(
    std::int32_t slot, /* RefCounted */ ConstPtr<ByteArray> advertisement) {
  Synchronized s(lock_.get());

  ScopedPtr<ConstPtr<ByteArray>> scoped_advertisement(advertisement);

  // Blindly remove from the advertisements map to make sure any existing
  // key-value pair is destroyed.
  eraseOwnedPtrFromMap(advertisements_, slot);

  advertisements_.insert(std::make_pair(slot, scoped_advertisement.release()));
}

// Determines whether or not an advertisement was successfully read at the
// specified slot.
template <typename Platform>
bool AdvertisementReadResult<Platform>::hasAdvertisement(std::int32_t slot) {
  Synchronized s(lock_.get());

  return advertisements_.find(slot) != advertisements_.end();
}

// Retrieves all raw advertisements that were successfully read.
template <typename Platform>
std::set<ConstPtr<ByteArray>>
AdvertisementReadResult<Platform>::getAdvertisements() {
  Synchronized s(lock_.get());

  std::set<ConstPtr<ByteArray>> all_advertisements;
  for (AdvertisementMap::iterator it = advertisements_.begin();
       it != advertisements_.end(); ++it) {
    all_advertisements.insert(it->second);
  }

  return all_advertisements;
}

// Determines what stage we're in for retrying a read from an advertisement
// GATT server.
template <typename Platform>
typename AdvertisementReadResult<Platform>::RetryStatus::Value
AdvertisementReadResult<Platform>::evaluateRetryStatus() {
  Synchronized s(lock_.get());

  // Check if we have already succeeded reading this advertisement.
  if (result_ == Result::SUCCESS) {
    return RetryStatus::PREVIOUSLY_SUCCEEDED;
  }

  // Check if we have recently failed to read this advertisement.
  if (getDurationSinceReadMillis() < backoff_duration_millis_) {
    return RetryStatus::TOO_SOON;
  }

  return RetryStatus::RETRY;
}

// Records the status of the latest read, and updates the next backoff
// duration for subsequent reads. Be sure to also call
// {@link #addAdvertisement(int, byte[])} if any advertisements were read.
template <typename Platform>
void AdvertisementReadResult<Platform>::recordLastReadStatus(bool is_success) {
  Synchronized s(lock_.get());

  // Update the last read timestamp.
  last_read_timestamp_millis_ = system_clock_->elapsedRealtime();

  // Update the backoff duration.
  if (is_success) {
    // Reset the backoff duration now that we had a successful read.
    backoff_duration_millis_ = kAdvertisementBaseBackoffDurationMillis;
  } else {
    // Determine whether or not we were already failing before. If we were, we
    // should increase the backoff duration.
    if (result_ == Result::FAILURE) {
      // Use exponential backoff to determine the next backoff duration. This
      // simply involves multiplying our current backoff duration by some
      // multiplier.
      std::int64_t next_backoff_duration =
          kAdvertisementBackoffMultiplier * backoff_duration_millis_;
      // Update the backoff duration, making sure not to blow past the
      // ceiling.
      backoff_duration_millis_ = std::min(
          next_backoff_duration, kAdvertisementMaxBackoffDurationMillis);
    } else {
      // This is our first time failing, so we should only backoff for the
      // initial duration.
      backoff_duration_millis_ = kAdvertisementBaseBackoffDurationMillis;
    }
  }

  // Update the internal result.
  result_ = is_success ? Result::SUCCESS : Result::FAILURE;
}

// Returns how much time has passed since we last tried reading from an
// advertisement GATT server.
template <typename Platform>
std::int64_t AdvertisementReadResult<Platform>::getDurationSinceReadMillis() {
  Synchronized s(lock_.get());

  return system_clock_->elapsedRealtime() - last_read_timestamp_millis_;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
