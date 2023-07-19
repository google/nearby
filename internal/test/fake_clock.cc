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

#include "internal/test/fake_clock.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace nearby {

FakeClock::~FakeClock() {
  absl::MutexLock lock(&mutex_);
  observers_.clear();
}

absl::Time FakeClock::Now() const {
  absl::MutexLock lock(&mutex_);
  return now_;
}

void FakeClock::AddObserver(absl::string_view name,
                            std::function<void()> observer) {
  absl::MutexLock lock(&mutex_);
  observers_.emplace(name, std::move(observer));
}

void FakeClock::RemoveObserver(absl::string_view name) {
  absl::MutexLock lock(&mutex_);
  observers_.erase(name);
}

void FakeClock::FastForward(absl::Duration duration) {
  std::vector<std::string> timer_callback_ids;
  {
    absl::MutexLock lock(&mutex_);
    now_ += duration;
    for (const auto& observer : observers_) {
      timer_callback_ids.push_back(observer.first);
    }
  }

  for (const auto& timer_callback_id : timer_callback_ids) {
    // Timer may be closed during other timer running, so only run callback
    // on available timers.
    bool is_alive_timer = true;
    std::function<void()> callback;

    {
      absl::MutexLock lock(&mutex_);
      is_alive_timer = observers_.contains(timer_callback_id);
      if (!is_alive_timer) {
        continue;
      }
      callback = observers_[timer_callback_id];
    }

    callback();
  }
}

int FakeClock::GetObserversCount() {
  absl::MutexLock lock(&mutex_);
  return observers_.size();
}

void FakeClock::Reset() {
  absl::MutexLock lock(&mutex_);
  return observers_.clear();
}

}  // namespace nearby
