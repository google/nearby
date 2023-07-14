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

#include "internal/test/fake_timer.h"

#include <functional>
#include <string>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {

FakeTimer::FakeTimer(FakeClock* clock) : clock_(clock) {
  id_ = std::to_string(absl::ToUnixNanos(absl::Now()));
  clock_->AddObserver(id_, [this]() { ClockUpdated(); });
}

FakeTimer::~FakeTimer() { clock_->RemoveObserver(id_); }

bool FakeTimer::Start(int delay, int period,
                      absl::AnyInvocable<void()> callback) {
  if (is_started_ || delay < 0 || period < 0) {
    return false;
  }
  delay_ = delay;
  period_ = period;
  callback_ = std::move(callback);
  start_time_ = clock_->Now();
  fired_count_ = 0;
  is_started_ = true;

  return true;
}

bool FakeTimer::Stop() {
  is_started_ = false;
  return true;
}

bool FakeTimer::IsRunning() { return is_started_; }

void FakeTimer::ClockUpdated() {
  if (!is_started_) {
    return;
  }

  absl::Time now = clock_->Now();
  int64_t duration = absl::ToInt64Milliseconds(now - start_time_);
  FakeClock* clock = clock_;
  // The timer may be released during callback, save period and delay to avoid
  // access exception.
  int period = period_;
  int delay = delay_;

  if (duration >= delay_ && fired_count_ == 0) {
    ++fired_count_;
    if (callback_ != nullptr) {
      callback_();
    }
  }

  if (period == 0 || duration < delay ||
      ((clock_ != nullptr) && (clock_ != clock))) {
    return;
  }

  int count = (duration - delay) / period;
  int should_fire_count = count - fired_count_ + 1;
  for (int i = 0; i < should_fire_count; ++i) {
    ++fired_count_;
    if (callback_ != nullptr) {
      callback_();
    }
  }
}

bool FakeTimer::FireNow() {
  if (IsRunning() && callback_) {
    callback_();
    return true;
  }

  return false;
}

}  // namespace nearby
