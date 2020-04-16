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

#ifndef CORE_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_
#define CORE_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_

#include <cstdint>
#include <map>
#include <set>

#include "platform/api/lock.h"
#include "platform/api/system_clock.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Representation of a GATT advertisement read result. This object helps us
// determine whether or not we need to retry GATT reads.
template <typename Platform>
class AdvertisementReadResult {
 public:
  AdvertisementReadResult();
  ~AdvertisementReadResult();

  struct RetryStatus {
    enum Value {
      UNKNOWN = 0,
      RETRY = 1,
      PREVIOUSLY_SUCCEEDED = 2,
      TOO_SOON = 3,
    };
  };

  void addAdvertisement(std::int32_t slot, ConstPtr<ByteArray> advertisement);
  bool hasAdvertisement(std::int32_t slot);
  std::set<ConstPtr<ByteArray>> getAdvertisements();
  typename RetryStatus::Value evaluateRetryStatus();
  void recordLastReadStatus(bool is_success);
  std::int64_t getDurationSinceReadMillis();

 private:
  struct Result {
    enum Value { UNKNOWN = 0, SUCCESS = 1, FAILURE = 2 };
  };

  static const float kAdvertisementBackoffMultiplier;
  static const std::int64_t kAdvertisementBaseBackoffDurationMillis;
  static const std::int64_t kAdvertisementMaxBackoffDurationMillis;

  // ------------ GENERAL ------------
  ScopedPtr<Ptr<Lock>> lock_;
  ScopedPtr<Ptr<SystemClock>> system_clock_;

  // ------ ADVERTISEMENTREADRESULT STATE ------
  // Maps slot numbers to the GATT advertisement found in that slot.
  typedef std::map<std::int32_t, /* RefCounted */ ConstPtr<ByteArray>>
      AdvertisementMap;
  AdvertisementMap advertisements_;

  std::int64_t backoff_duration_millis_;
  std::int64_t last_read_timestamp_millis_;
  typename Result::Value result_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/advertisement_read_result.cc"

#endif  // CORE_INTERNAL_MEDIUMS_ADVERTISEMENT_READ_RESULT_H_
