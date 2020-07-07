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

#ifndef PLATFORM_V2_PUBLIC_CANCELABLE_ALARM_H_
#define PLATFORM_V2_PUBLIC_CANCELABLE_ALARM_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "platform_v2/public/cancelable.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/mutex_lock.h"
#include "platform_v2/public/scheduled_executor.h"

namespace location {
namespace nearby {

/**
 * A cancelable alarm with a name. This is a simple wrapper around the logic
 * for posting a Runnable on a ScheduledExecutor and (possibly) later
 * canceling it.
 */
class CancelableAlarm {
 public:
  CancelableAlarm() = default;
  CancelableAlarm(absl::string_view name, std::function<void()>&& runnable,
                  absl::Duration delay, ScheduledExecutor* scheduled_executor)
      : name_(name),
        cancelable_(scheduled_executor->Schedule(std::move(runnable), delay)) {}
  ~CancelableAlarm() = default;
  CancelableAlarm(CancelableAlarm&& other) {
    *this = std::move(other);
  }
  CancelableAlarm& operator=(CancelableAlarm&& other) {
    MutexLock lock(&mutex_);
    {
      MutexLock other_lock(&other.mutex_);
      name_ = std::move(other.name_);
      cancelable_ = std::move(other.cancelable_);
    }
    return *this;
  }

  bool Cancel() {
    MutexLock lock(&mutex_);
    return cancelable_.Cancel();
  }

  bool IsValid() {
    return cancelable_.IsValid();
  }

 private:
  Mutex mutex_;
  std::string name_;
  Cancelable cancelable_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_CANCELABLE_ALARM_H_
