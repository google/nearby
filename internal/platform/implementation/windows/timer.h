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

#ifndef PLATFORM_IMPL_WINDOWS_TIMER_H_
#define PLATFORM_IMPL_WINDOWS_TIMER_H_

#include <windows.h>

#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace windows {

class Timer : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override;
  bool Stop() override;
  bool FireNow() override;

 private:
  static void CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);

  int delay_;
  int interval_;
  absl::AnyInvocable<void()> callback_;
  HANDLE handle_ = NULL;
  HANDLE timer_queue_handle_ = NULL;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_TIMER_H_
