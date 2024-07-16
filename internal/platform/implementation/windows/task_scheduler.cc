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

#include "internal/platform/implementation/windows/task_scheduler.h"

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"

namespace nearby::windows {
namespace {
void CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
  Runnable* task = reinterpret_cast<Runnable*>(lpParam);
  if (task != nullptr) {
    (*task)();
  }
}
}  // namespace

TaskScheduler::TaskScheduler() {
  NEARBY_LOGS(INFO) << __func__ << ": Created task scheduler: " << this;
}
TaskScheduler::~TaskScheduler() {
  Shutdown();
  NEARBY_LOGS(INFO) << __func__ << ": Destroyed task scheduler: " << this;
}

std::shared_ptr<api::Cancelable> TaskScheduler::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  return Schedule(std::move(runnable), duration, absl::ZeroDuration());
}

std::shared_ptr<api::Cancelable> TaskScheduler::Schedule(
    Runnable&& runnable, absl::Duration duration,
    absl::Duration repeat_interval) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__
                    << ": Scheduling task on task scheduler:" << this
                    << ", duration: " << absl::ToInt64Milliseconds(duration)
                    << "ms, repeat_interval: "
                    << absl::ToInt64Milliseconds(repeat_interval) << "ms";

  std::shared_ptr<ScheduledTask> task =
      std::make_shared<ScheduledTask>(*this, std::move(runnable));

  HANDLE timer_handle = nullptr;
  if (!CreateTimerQueueTimer(&timer_handle, nullptr,
                             static_cast<WAITORTIMERCALLBACK>(TimerRoutine),
                             task->runnable(),
                             absl::ToInt64Milliseconds(duration),
                             absl::ToInt64Milliseconds(repeat_interval), 0)) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to create timer queue timer in task scheduler:" << this
        << " error: " << GetLastError();
    return nullptr;
  }

  task->SetTimerHandle(reinterpret_cast<intptr_t>(timer_handle));
  scheduled_tasks_.insert({reinterpret_cast<intptr_t>(timer_handle), task});
  NEARBY_LOGS(INFO) << __func__ << ": Scheduled task " << task.get()
                    << " on task scheduler:" << this
                    << " timer handle: " << task->timer_handle();
  return task;
}

void TaskScheduler::Shutdown() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__ << ": Shutting down task scheduler:" << this;
  if (is_shutdown_) {
    return;
  }
  for (auto& task : scheduled_tasks_) {
    // Wait for running task to finish.
    if (!DeleteTimerQueueTimer(
            nullptr, reinterpret_cast<HANDLE>(task.second->timer_handle()),
            INVALID_HANDLE_VALUE)) {
      if (GetLastError() != ERROR_IO_PENDING) {
        NEARBY_LOGS(ERROR) << __func__
                           << ": Failed to delete timer queue timer: "
                           << task.second->timer_handle()
                           << " error: " << GetLastError();
      }
    }
  }
  scheduled_tasks_.clear();
  is_shutdown_ = true;
  NEARBY_LOGS(INFO) << __func__ << ": Shut down task scheduler:" << this;
}

TaskScheduler::ScheduledTask::ScheduledTask(TaskScheduler& task_scheduler,
                                            Runnable&& runnable)
    : task_scheduler_(&task_scheduler), runnable_(std::move(runnable)) {}

bool TaskScheduler::ScheduledTask::Cancel() {
  NEARBY_LOGS(INFO) << __func__ << ": Cancelling timer " << timer_handle_
                    << " from task scheduler:" << this;
  return task_scheduler_->remove_scheduled_task(timer_handle_);
}

void TaskScheduler::ScheduledTask::SetTimerHandle(intptr_t timer_handle) {
  timer_handle_ = timer_handle;
}

Runnable* TaskScheduler::ScheduledTask::runnable() { return &runnable_; }

intptr_t TaskScheduler::ScheduledTask::timer_handle() { return timer_handle_; }

bool TaskScheduler::remove_scheduled_task(intptr_t timer_handle) {
  MutexLock lock(&mutex_);
  auto it = scheduled_tasks_.find(timer_handle);
  if (it == scheduled_tasks_.end()) {
    return false;
  }

  // Wait for running task to finish.
  if (!DeleteTimerQueueTimer(nullptr, reinterpret_cast<HANDLE>(timer_handle),
                             INVALID_HANDLE_VALUE)) {
    if (GetLastError() != ERROR_IO_PENDING) {
      NEARBY_LOGS(ERROR) << __func__ << ": Failed to delete timer queue timer: "
                         << timer_handle << " error: " << GetLastError();
    }
  }

  scheduled_tasks_.erase(it);
  return true;
}

}  // namespace nearby::windows
