// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_CLOCK_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_CLOCK_H_

#include <functional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"

namespace nearby {

class FakeClock : public Clock {
 public:
  FakeClock() { now_ = absl::Now(); }
  FakeClock(FakeClock&&) = default;
  FakeClock& operator=(FakeClock&&) = default;
  ~FakeClock() override ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Time Now() const override;

  void AddObserver(absl::string_view name, std::function<void()> observer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void RemoveObserver(absl::string_view name) ABSL_LOCKS_EXCLUDED(mutex_);

  void FastForward(absl::Duration duration);

  int GetObserversCount() ABSL_LOCKS_EXCLUDED(mutex_);

  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable absl::Mutex mutex_;
  absl::Time now_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::function<void()>> observers_
      ABSL_GUARDED_BY(mutex_);
};
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_CLOCK_H_
