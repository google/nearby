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

#include <stdbool.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
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
                  absl::Duration delay, ScheduledExecutor* scheduled_executor);
  ~CancelableAlarm();

  void Cancel();
  bool IsRunning();

  bool IsValid();

 private:
  const std::string name_;
  // The timer state is dependent on the order of expiration and cancellation.
  // The |run_cnt_| is used to track what order these tasks occured.
  // The task that runs last will be responsible for deleting the |run_cnt_|.
  std::atomic<int8_t>* run_cnt_ = nullptr;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CANCELABLE_ALARM_H_
