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

#include "internal/platform/implementation/timer.h"
#import "GoogleToolboxForMac/GTMLogger.h"

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
    GTMLoggerError(@"Delay and interval must be positive or zero.");
    return false;
  }

  if (timer_ != nil) {
    GTMLoggerError(@"Timer has already started.");
    return false;
  }

  uint64_t delayInNanoseconds = delay * NSEC_PER_MSEC;
  // If `interval` is 0, it means we only want the timer to fire once. We map this to
  // `DISPATCH_TIME_FOREVER` to have this effect.
  uint64_t intervalInNanoseconds = interval == 0 ? DISPATCH_TIME_FOREVER : interval * NSEC_PER_MSEC;
  callback_ = std::move(callback);

  timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, /*handle=*/0, /*mask=*/0,
                                  dispatch_get_main_queue());

  dispatch_source_set_event_handler(timer_, ^{
    // If our interval is `DISPATCH_TIME_FOREVER`, it means we only want the timer to fire once.
    // We need to cancel the timer before the callback is called since the `Timer` object may no
    // longer exist immediately after invoking the callback.
    if (intervalInNanoseconds == DISPATCH_TIME_FOREVER && timer_ != nil) {
      dispatch_source_cancel(timer_);
      timer_ = nil;
    }

    if (callback_ != nil) {
      callback_();
    }
  });

  dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, delayInNanoseconds),
                            intervalInNanoseconds, GNCTimerLeewayInNanoseconds);
  dispatch_resume(timer_);
  return true;
}

bool Timer::Stop() {
  if (timer_ != nil) {
    dispatch_source_cancel(timer_);
  }
  timer_ = nil;
  callback_ = nil;
  return true;
}

bool Timer::FireNow() {
  if (callback_ != nil) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
      callback_();
    });
    return true;
  }
  return false;
}

}  // namespace apple
}  // namespace nearby
