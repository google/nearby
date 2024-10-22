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

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/logging.h"
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
  LOG(INFO) << __func__ << ": Created task scheduler: " << this;
}

TaskScheduler::~TaskScheduler() {
  Shutdown();
  LOG(INFO) << __func__ << ": Destroyed task scheduler: " << this;
}

std::shared_ptr<api::Cancelable> TaskScheduler::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  return Schedule(std::move(runnable), duration, absl::ZeroDuration());
}

std::shared_ptr<api::Cancelable> TaskScheduler::Schedule(
    Runnable&& runnable, absl::Duration duration,
    absl::Duration repeat_interval) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Scheduling task on task scheduler:" << this
            << ", duration: " << absl::ToInt64Milliseconds(duration)
            << "ms, repeat_interval: "
            << absl::ToInt64Milliseconds(repeat_interval) << "ms";
  if (is_shutdown_) {
    LOG(ERROR) << __func__
               << ": Attempt to schedule task on a shut down task "
                  "scheduler: "
               << this;
    return nullptr;
  }

  // Clear all cancelled tasks.
  CleanScheduledTasks();

  std::shared_ptr<ScheduledTask> task = std::make_shared<ScheduledTask>(
      *this, std::move(runnable), repeat_interval != absl::ZeroDuration());

  HANDLE timer_handle = nullptr;
  if (!CreateTimerQueueTimer(&timer_handle, nullptr,
                             static_cast<WAITORTIMERCALLBACK>(TimerRoutine),
                             task->runnable(),
                             absl::ToInt64Milliseconds(duration),
                             absl::ToInt64Milliseconds(repeat_interval), 0)) {
    LOG(ERROR) << __func__
               << ": Failed to create timer queue timer in task scheduler:"
               << this << " error: " << GetLastError();
    return nullptr;
  }

  task->set_timer_handle(reinterpret_cast<intptr_t>(timer_handle));
  scheduled_tasks_.insert({reinterpret_cast<intptr_t>(timer_handle), task});
  LOG(INFO) << __func__ << ": Scheduled task " << task.get()
            << " on task scheduler:" << this
            << " timer handle: " << task->timer_handle();
  return task;
}

void TaskScheduler::Shutdown() {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Shutting down task scheduler:" << this;
  if (is_shutdown_) {
    return;
  }
  for (auto& task : scheduled_tasks_) {
    if (task.second->is_cancelled()) {
      continue;
    }
    // Wait for running task to finish.
    if (!DeleteTimerQueueTimer(
            nullptr, reinterpret_cast<HANDLE>(task.second->timer_handle()),
            INVALID_HANDLE_VALUE)) {
      if (GetLastError() != ERROR_IO_PENDING) {
        LOG(ERROR) << __func__ << ": Failed to delete timer queue timer: "
                   << task.second->timer_handle()
                   << " error: " << GetLastError();
      }
    }
  }
  scheduled_tasks_.clear();
  is_shutdown_ = true;
  LOG(INFO) << __func__ << ": Shut down task scheduler:" << this;
}

TaskScheduler::ScheduledTask::ScheduledTask(TaskScheduler& task_scheduler,
                                            Runnable&& runnable,
                                            bool is_repeated)
    : task_scheduler_(&task_scheduler), is_repeated_(is_repeated) {
  runnable_ = [this, runnable = std::move(runnable)]() mutable {
    {
      absl::MutexLock lock(&mutex_);
      is_executed_ = true;
    }
    if (runnable) {
      runnable();
    }
  };
}

bool TaskScheduler::ScheduledTask::Cancel() {
  LOG(INFO) << __func__ << ": Cancelling timer " << timer_handle()
            << " from task scheduler:" << this;
  {
    absl::MutexLock lock(&mutex_);
    if (is_cancelled_) {
      return false;
    }
    is_cancelled_ = true;
  }

  bool result = task_scheduler_->CancelScheduledTask(timer_handle());
  {
    absl::MutexLock lock(&mutex_);
    if (!is_repeated_ && is_executed_) {
      result = false;
    }
  }
  return result;
}

void TaskScheduler::ScheduledTask::set_timer_handle(intptr_t timer_handle) {
  absl::MutexLock lock(&mutex_);
  timer_handle_ = timer_handle;
}

Runnable* TaskScheduler::ScheduledTask::runnable() {
  absl::MutexLock lock(&mutex_);
  return &runnable_;
}

intptr_t TaskScheduler::ScheduledTask::timer_handle() const {
  absl::MutexLock lock(&mutex_);
  return timer_handle_;
}

bool TaskScheduler::ScheduledTask::is_cancelled() const {
  absl::MutexLock lock(&mutex_);
  return is_cancelled_;
}

bool TaskScheduler::CancelScheduledTask(intptr_t timer_handle) {
  absl::MutexLock lock(&mutex_);
  auto it = scheduled_tasks_.find(timer_handle);
  if (it == scheduled_tasks_.end()) {
    return false;
  }

  // Wait for running task to finish.
  if (!DeleteTimerQueueTimer(nullptr, reinterpret_cast<HANDLE>(timer_handle),
                             INVALID_HANDLE_VALUE)) {
    if (GetLastError() != ERROR_IO_PENDING) {
      LOG(ERROR) << __func__
                 << ": Failed to delete timer queue timer: " << timer_handle
                 << " error: " << GetLastError();
      return false;
    }
  }

  return true;
}

void TaskScheduler::CleanScheduledTasks() {
  auto it = scheduled_tasks_.begin();
  while (it != scheduled_tasks_.end()) {
    if (it->second->is_cancelled()) {
      scheduled_tasks_.erase(it++);
    } else {
      ++it;
    }
  }
}

}  // namespace nearby::windows
