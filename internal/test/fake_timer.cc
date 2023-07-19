// Copyright 2021-2023 Google LLC
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

#include "absl/functional/any_invocable.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

FakeTimer::FakeTimer(FakeClock* clock) : clock_(clock) {}

FakeTimer::~FakeTimer() { Stop(); }

bool FakeTimer::Start(int delay, int period,
                      absl::AnyInvocable<void()> callback) {
  MutexLock lock(&mutex_);
  return InternalStart(delay, period, std::move(callback));
}

bool FakeTimer::Stop() {
  MutexLock lock(&mutex_);
  return InternalStop();
}

bool FakeTimer::IsRunning() {
  MutexLock lock(&mutex_);
  return !timer_data_.id.empty();
}

void FakeTimer::ClockUpdated() {
  MutexLock lock(&mutex_);
  TimerData timer_data = timer_data_;
  if (timer_data.id.empty()) {
    return;
  }

  absl::Time now = clock_->Now();
  if (!timer_data.is_delay_called &&
      timer_data.last_time == timer_data.create_time &&
      timer_data.last_time + absl::Milliseconds(timer_data.delay) <= now) {
    timer_data.callback();
    if (timer_data.id == timer_data_.id) {
      timer_data.is_delay_called = true;
      timer_data.last_time =
          timer_data.last_time + absl::Milliseconds(timer_data.delay);
      timer_data_ = timer_data;
    }
  }

  if (timer_data.period == 0 || timer_data.id != timer_data_.id) {
    // timer is changed during callback.
    return;
  }

  timer_data_ = timer_data;
  while (timer_data.last_time + absl::Milliseconds(timer_data.period) <= now) {
    timer_data.callback();
    if (timer_data.id == timer_data_.id) {
      timer_data.last_time += absl::Milliseconds(timer_data.period);
    } else {
      return;
    }
  }

  timer_data_ = timer_data;
}

bool FakeTimer::FireNow() {
  if (IsRunning()) {
    timer_data_.callback();
    return true;
  }

  return false;
}

bool FakeTimer::InternalStart(int delay, int period,
                              absl::AnyInvocable<void()> callback) {
  if (delay < 0 || period < 0 || callback == nullptr) {
    return false;
  }

  if (!timer_data_.id.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": timer is already running";
    return false;
  }

  timer_data_.id = std::to_string(absl::ToUnixNanos(absl::Now()));
  timer_data_.delay = delay;
  timer_data_.period = period;
  callback_ = std::move(callback);
  timer_data_.callback = [&]() { callback_(); };
  timer_data_.create_time = clock_->Now();
  timer_data_.last_time = timer_data_.create_time;
  clock_->AddObserver(timer_data_.id, [this]() { ClockUpdated(); });
  return true;
}

bool FakeTimer::InternalStop() {
  if (timer_data_.id.empty()) {
    return true;
  }
  clock_->RemoveObserver(timer_data_.id);
  timer_data_ = {};
  return true;
}

}  // namespace nearby
