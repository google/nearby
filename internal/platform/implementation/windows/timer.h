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

#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/timer.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/implementation/windows/task_scheduler.h"

namespace nearby {
namespace windows {

class Timer : public api::Timer {
 public:
  Timer();
  ~Timer() override;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool Stop() override ABSL_LOCKS_EXCLUDED(mutex_);
  bool FireNow() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  static void CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);

  mutable absl::Mutex mutex_;
  const bool use_task_scheduler_;
  int delay_ ABSL_GUARDED_BY(mutex_);
  int interval_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> callback_;
  HANDLE handle_ ABSL_GUARDED_BY(mutex_) = nullptr;
  HANDLE timer_queue_handle_ ABSL_GUARDED_BY(mutex_) = nullptr;
  std::unique_ptr<SubmittableExecutor> task_executor_ ABSL_GUARDED_BY(mutex_) =
      nullptr;
  TaskScheduler task_scheduler_ ABSL_GUARDED_BY(mutex_);
  std::shared_ptr<api::Cancelable> cancelable_task_ ABSL_GUARDED_BY(mutex_) =
      nullptr;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_TIMER_H_
