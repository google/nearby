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

#ifndef PLATFORM_PUBLIC_CANCELABLE_ALARM_H_
#define PLATFORM_PUBLIC_CANCELABLE_ALARM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "internal/platform/cancelable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/scheduled_executor.h"

namespace nearby {

/**
 * A cancelable alarm with a name. This is a simple wrapper around the logic
 * for posting a Runnable on a ScheduledExecutor and (possibly) later
 * canceling it.
 */
class CancelableAlarm {
 public:
  CancelableAlarm() = default;
  CancelableAlarm(absl::string_view name, absl::AnyInvocable<void()>&& runnable,
                  absl::Duration delay, ScheduledExecutor* scheduled_executor,
                  bool is_recurring = false)
      : name_(name), scheduled_executor_(scheduled_executor), delay_(delay) {
    if (is_recurring) {
      runnable_ = std::move(runnable);
      Schedule();
    } else {
      cancelable_ = scheduled_executor_->Schedule(std::move(runnable), delay_);
    }
  }
  ~CancelableAlarm() = default;

  bool Cancel() {
    MutexLock lock(&mutex_);
    return cancelable_.Cancel();
  }

  bool IsValid() {
    MutexLock lock(&mutex_);
    return cancelable_.IsValid();
  }

 private:
  void Schedule() {
    MutexLock lock(&mutex_);
    cancelable_ = scheduled_executor_->Schedule(
        [this]() {
          runnable_();
          Schedule();
        },
        delay_);
  }

  Mutex mutex_;
  std::string name_;
  Cancelable cancelable_;
  ScheduledExecutor* scheduled_executor_;
  absl::Duration delay_;
  absl::AnyInvocable<void()> runnable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CANCELABLE_ALARM_H_
