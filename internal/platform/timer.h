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

#ifndef PLATFORM_PUBLIC_TIMER_H_
#define PLATFORM_PUBLIC_TIMER_H_

#include "absl/functional/any_invocable.h"

namespace nearby {
class Timer {
 public:
  virtual ~Timer() = default;

  // Starts the timer.
  //
  // @param delay  The amount of time in milliseconds relative to the current
  //        time that must elapse before the timer is signaled for the first
  //        time.
  // @param period The period of the timer, in milliseconds. If this parameter
  //        is zero, the timer is signaled once. If this parameter is greater
  //        than zero, the timer is periodic.
  // @param callback The callback is called when timer is signaled
  //
  // @return Returns true if succeed, otherwise false is returned.
  virtual bool Start(int delay, int period,
                     absl::AnyInvocable<void()> callback) = 0;
  virtual bool Stop() = 0;
  virtual bool IsRunning() = 0;
  virtual bool FireNow() = 0;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_TIMER_H_
