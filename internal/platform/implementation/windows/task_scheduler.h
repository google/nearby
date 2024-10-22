// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_TASK_SCHEDULER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_TASK_SCHEDULER_H_

#include <cstdint>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/runnable.h"

namespace nearby::windows {

// TaskScheduler is a utility class to scheduled a runnable task. It is used by
// the ScheduledExecutor and timer implementations.
class TaskScheduler {
 public:
  TaskScheduler();
  ~TaskScheduler();

  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration);

  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration,
                                            absl::Duration repeat_interval)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  class ScheduledTask : public api::Cancelable {
   public:
    explicit ScheduledTask(TaskScheduler& task_scheduler, Runnable&& runnable,
                           bool is_repeated);
    ~ScheduledTask() override = default;

    // Note: not support to cancel and shutdown a scheduled task in the
    // callback.
    // when task is cancelled or executed, return false, otherwise return true.
    bool Cancel() override ABSL_LOCKS_EXCLUDED(mutex_);
    bool is_cancelled() const ABSL_LOCKS_EXCLUDED(mutex_);

    intptr_t timer_handle() const ABSL_LOCKS_EXCLUDED(mutex_);
    void set_timer_handle(intptr_t timer_handle) ABSL_LOCKS_EXCLUDED(mutex_);
    Runnable* runnable() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    mutable absl::Mutex mutex_;
    TaskScheduler* const task_scheduler_;
    Runnable runnable_ ABSL_GUARDED_BY(mutex_);
    intptr_t timer_handle_ ABSL_GUARDED_BY(mutex_);
    bool is_cancelled_ ABSL_GUARDED_BY(mutex_) = false;
    bool is_executed_ ABSL_GUARDED_BY(mutex_) = false;
    bool is_repeated_ ABSL_GUARDED_BY(mutex_) = false;
  };

  bool CancelScheduledTask(intptr_t timer_handle) ABSL_LOCKS_EXCLUDED(mutex_);
  void CleanScheduledTasks() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  bool is_shutdown_ ABSL_GUARDED_BY(mutex_) = false;
  absl::flat_hash_map<intptr_t, std::shared_ptr<ScheduledTask>> scheduled_tasks_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_TASK_SCHEDULER_H_
