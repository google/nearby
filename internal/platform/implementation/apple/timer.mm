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

#import "internal/platform/implementation/apple/timer.h"

#include <utility>

#include "absl/synchronization/mutex.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#include "internal/platform/implementation/timer.h"

// The leeway parameter is a hint from the application as to the amount of time, in nanoseconds, up
// to which the system can defer the timer to align with other system activity for improved system
// performance or power consumption. For example, an application might perform a periodic task every
// 5 minutes, with a leeway of up to 30 seconds. Note that some latency is to be expected for all
// timers, even when a leeway value of zero is specified.
uint64_t const GNCTimerLeewayInNanoseconds = 0;

namespace nearby {
namespace apple {

Timer::~Timer() { Stop(); }

bool Timer::Create(int delay, int interval, absl::AnyInvocable<void()> callback) {
  if (delay < 0 || interval < 0) {
    GNCLoggerError(@"Delay and interval must be positive or zero.");
    return false;
  }

  absl::MutexLock lock(mutex_);
  if (timer_ != nullptr) {
    GNCLoggerError(@"Timer has already started.");
    return false;
  }

  uint64_t delayInNanoseconds = delay * NSEC_PER_MSEC;
  // If `interval` is 0, it means we only want the timer to fire once. We map this to
  // `DISPATCH_TIME_FOREVER` to have this effect.
  uint64_t intervalInNanoseconds = interval == 0 ? DISPATCH_TIME_FOREVER : interval * NSEC_PER_MSEC;
  callback_ = std::move(callback);

  // Run the timer on a background queue to avoid deadlocking the main thread,
  // which may be blocked waiting for a future to complete.
  timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, /*handle=*/0, /*mask=*/0,
                                  dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));

  dispatch_source_set_event_handler(timer_, ^{
    absl::AnyInvocable<void()> callback_to_run = nullptr;
    bool is_one_shot = (intervalInNanoseconds == DISPATCH_TIME_FOREVER);
    {
      absl::MutexLock lock(mutex_);
      // If Stop() was called concurrently, the callback will be null.
      if (!callback_ || callback_running_) {
        return;
      }
      callback_running_ = true;
      if (is_one_shot && timer_ != nullptr) {
        dispatch_source_cancel(timer_);
        timer_ = nullptr;
      }
      callback_to_run = std::move(callback_);
    }

    if (callback_to_run) {
      callback_to_run();
    }
    {
      absl::MutexLock lock(mutex_);
      if (!is_one_shot && callback_to_run) {
        // For periodic timers, move the callback back for the next run.
        callback_ = std::move(callback_to_run);
      }
      callback_running_ = false;
      condvar_.SignalAll();
    }
  });

  dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, delayInNanoseconds),
                            intervalInNanoseconds, GNCTimerLeewayInNanoseconds);
  dispatch_resume(timer_);
  return true;
}

bool Timer::Stop() {
  absl::MutexLock lock(mutex_);
  if (timer_ != nullptr) {
    dispatch_source_cancel(timer_);
    timer_ = nullptr;
  }
  // Wait for a potentially running callback to finish before destroying it.
  while (callback_running_) {
    condvar_.Wait(&mutex_);
  }
  callback_ = nullptr;
  return true;
}

}  // namespace apple
}  // namespace nearby
