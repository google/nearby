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

#ifndef PLATFORM_IMPL_WINDOWS_SCHEDULED_EXECUTOR_H_
#define PLATFORM_IMPL_WINDOWS_SCHEDULED_EXECUTOR_H_

#include <windows.h>

#include <memory>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/windows/executor.h"

namespace nearby {
namespace windows {

#define TIMER_NAME_BUFFER_SIZE 64

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public api::ScheduledExecutor {
 public:
  ScheduledExecutor();

  ~ScheduledExecutor() override = default;

  // Cancelable is kept both in the executor context, and in the caller context.
  // We want Cancelable to live until both caller and executor are done with it.
  // Exclusive ownership model does not work for this case;
  // using std:shared_ptr<> instead if std::unique_ptr<>.
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration) override;

  // Executes the runnable task immedately.
  void Execute(Runnable&& runnable) override;

  // Shutdowns the executor, all scheduled task will be cancelled.
  void Shutdown() override;

 private:
  class ScheduledTask : public api::Cancelable {
   public:
    explicit ScheduledTask(Runnable&& task, absl::Duration duration)
        : task_(std::move(task)), duration_(duration) {}

    bool Cancel() override {
      if (is_executed_) {
        return false;
      }

      is_cancelled_ = true;
      notification_.Notify();
      return true;
    };

    void Start() {
      if (is_executed_ ||
          notification_.WaitForNotificationWithTimeout(duration_)) {
        return;
      }

      is_executed_ = true;
      task_();
    }

    bool IsDone() const { return is_cancelled_ || is_executed_; }

   private:
    Runnable task_;
    absl::Duration duration_;
    absl::Notification notification_;
    bool is_cancelled_ = false;
    bool is_executed_ = false;
  };

  std::unique_ptr<nearby::windows::Executor> executor_ = nullptr;
  std::vector<std::shared_ptr<ScheduledTask>> scheduled_tasks_;
  std::atomic_bool shut_down_ = false;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_SCHEDULED_EXECUTOR_H_
