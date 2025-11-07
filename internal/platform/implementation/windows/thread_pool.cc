// Copyright 2021-2023 Google LLC
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

#include "internal/platform/implementation/windows/thread_pool.h"

#include <windows.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

std::unique_ptr<ThreadPool> ThreadPool::Create(uint32_t max_pool_size) {
  PTP_POOL thread_pool = nullptr;
  TP_CALLBACK_ENVIRON thread_pool_environ;
  PTP_CLEANUP_GROUP cleanup_group = nullptr;

  if (max_pool_size <= 0) {
    LOG(ERROR) << __func__
               << ": Maximum pool size must be positive integer value.";
    return nullptr;
  }

  InitializeThreadpoolEnvironment(&thread_pool_environ);
  cleanup_group = CreateThreadpoolCleanupGroup();
  if (cleanup_group == nullptr) {
    LOG(ERROR) << __func__
               << ": Failed to create thread pool cleanup group. LastError: "
               << GetLastError();
    return nullptr;
  }

  thread_pool = CreateThreadpool(nullptr);
  if (thread_pool == nullptr) {
    LOG(ERROR) << __func__ << ": Failed to create thread pool. LastError: "
               << GetLastError();
    CloseThreadpoolCleanupGroup(/*ptpcg=*/cleanup_group);
    return nullptr;
  }

  // Sets thread pool maximum value. In order to release all threads,
  // it will keep at least one thread.
  SetThreadpoolThreadMaximum(/*ptpp=*/thread_pool, /*cthrdMost=*/max_pool_size);
  if (!SetThreadpoolThreadMinimum(/*ptpp=*/thread_pool, /*cthrdMic=*/1)) {
    LOG(ERROR) << __func__
               << ": failed to set minimum thread pool size. LastError: "
               << GetLastError();
    CloseThreadpoolCleanupGroup(/*ptpcg=*/cleanup_group);
    CloseThreadpool(/*ptpp=*/thread_pool);
    return nullptr;
  }

  //
  // Associate the callback environment with our thread pool.
  //
  SetThreadpoolCallbackPool(/*pcbe=*/&thread_pool_environ,
                            /*ptpp=*/thread_pool);
  SetThreadpoolCallbackCleanupGroup(/*pcbe=*/&thread_pool_environ,
                                    /*ptpcg=*/cleanup_group,
                                    /*pfng=*/nullptr);

  return absl::WrapUnique(new ThreadPool(thread_pool, thread_pool_environ,
                                         cleanup_group, max_pool_size));
}

bool ThreadPool::Run(Runnable task) {
  absl::MutexLock lock(mutex_);

  if (task == nullptr) {
    LOG(WARNING) << __func__ << ": Invalid task.";
    return false;
  }

  if (is_shut_down_) {
    LOG(WARNING) << __func__ << ": Thread pool is shut down.";
    return false;
  }

  PTP_WORK work;

  work = CreateThreadpoolWork(/*pfnwk=*/WorkCallback, /*pv=*/this,
                              /*pcbe=*/&thread_pool_environ_);
  if (work == nullptr) {
    LOG(ERROR) << __func__ << ": failed to create thread pool work. LastError: "
               << GetLastError();
    return false;
  }

  VLOG(1) << __func__ << ": Scheduled to run work(" << work << ").";

  //
  // Submit the work to the pool. Because this was a pre-allocated
  // work item (using CreateThreadpoolWork), it is guaranteed to execute.
  //
  task_queue_.enqueue(std::move(task));
  SubmitThreadpoolWork(/*pwk=*/work);
  return true;
}

std::optional<uint64_t> ThreadPool::Run(Runnable task, absl::Duration delay) {
  return Run(std::move(task), delay, absl::ZeroDuration());
}

std::optional<uint64_t> ThreadPool::Run(Runnable task, absl::Duration delay,
                                        absl::Duration period) {
  absl::MutexLock lock(mutex_);

  if (task == nullptr) {
    LOG(WARNING) << __func__ << ": Invalid task.";
    return std::nullopt;
  }

  if (is_shut_down_) {
    LOG(WARNING) << __func__ << ": Thread pool is shut down.";
    return std::nullopt;
  }

  // Closing the timer within its callback is prohibited. The thread pool
  // ensures the callback remains active until its completion. When a new
  // delayed task arrives, all completed tasks are promptly cleaned. Developers
  // should cancel timers as soon as they are no longer needed.
  delayed_task_map_.clean_completed_tasks();

  PTP_TIMER timer = CreateThreadpoolTimer(
      /*pfnti=*/TimerCallback,
      /*pv=*/this,
      /*pcbe=*/&thread_pool_environ_);

  if (timer == nullptr) {
    LOG(WARNING) << __func__
                 << ": failed to create thread pool timer. LastError: "
                 << GetLastError();
    return std::nullopt;
  }

  VLOG(1) << __func__ << ": Scheduled to run timer(" << timer << ").";

  delayed_task_map_.put(
      timer, std::make_unique<DelayedTaskInfo>(std::move(task), delay, period));

  FILETIME file_due_time;
  ULARGE_INTEGER due_time;
  due_time.QuadPart = (ULONGLONG) - (absl::ToInt64Milliseconds(delay) * 10000);
  file_due_time.dwLowDateTime = due_time.LowPart;
  file_due_time.dwHighDateTime = due_time.HighPart;

  SetThreadpoolTimerEx(
      /*pti=*/timer, /*pftDueTime=*/&file_due_time,
      /*msPeriod=*/static_cast<DWORD>(absl::ToInt64Milliseconds(period)),
      /*msWindowLength=*/10);

  return reinterpret_cast<uint64_t>(timer);
}

void ThreadPool::ShutDown() {
  absl::MutexLock lock(mutex_);
  VLOG(1) << __func__ << ": Thread pool(" << this << ") is shutting down.";
  if (is_shut_down_) {
    LOG(INFO) << __func__ << ": Thread pool(" << this
              << ") is already shut down.";
    return;
  }

  is_shut_down_ = true;

  CloseThreadpoolCleanupGroupMembers(/*ptpcg=*/cleanup_group_,
                                     /*fCancelPendingCallbacks=*/FALSE,
                                     /*pvCleanupContext=*/nullptr);
  CloseThreadpoolCleanupGroup(/*ptpcg=*/cleanup_group_);
  CloseThreadpool(/*ptpp=*/thread_pool_);
  task_queue_.clear();
  delayed_task_map_.clear();
  VLOG(1) << __func__ << ": Thread pool(" << this << ") is shut down.";
}

ThreadPool::ThreadPool(PTP_POOL thread_pool,
                       TP_CALLBACK_ENVIRON thread_pool_environ,
                       PTP_CLEANUP_GROUP cleanup_group, int max_pool_size)
    : thread_pool_(thread_pool),
      thread_pool_environ_(thread_pool_environ),
      cleanup_group_(cleanup_group),
      max_pool_size_(max_pool_size) {
  VLOG(1) << __func__ << ": Thread pool(" << this
          << ") is created with size:" << max_pool_size_;
}

ThreadPool::~ThreadPool() {
  VLOG(1) << __func__ << ": Thread pool(" << this << ") is releasing.";
  ShutDown();
}

VOID CALLBACK ThreadPool::WorkCallback(PTP_CALLBACK_INSTANCE instance,
                                       PVOID parameter, PTP_WORK work) {
  // Instance is not used in work callbacks.
  UNREFERENCED_PARAMETER(instance);

  VLOG(1) << __func__ << ": Start to run work(" << work << ").";
  ThreadPool* thread_pool = static_cast<ThreadPool*>(parameter);
  thread_pool->RunNextTask();
  CloseThreadpoolWork(work);
  VLOG(1) << __func__ << ": Completed to run work(" << work << ").";
}

VOID CALLBACK ThreadPool::TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                        PVOID context, PTP_TIMER timer) {
  // Instance is not used in tiemr callbacks.
  UNREFERENCED_PARAMETER(instance);

  VLOG(1) << __func__ << ": Start to run timer(" << timer << ") callback.";
  ThreadPool* thread_pool = static_cast<ThreadPool*>(context);
  thread_pool->RunTimerCallback(timer);
  VLOG(1) << __func__ << ": Completed to run timer(" << timer << ") callback.";
}

void ThreadPool::RunNextTask() {
  std::optional<Runnable> task = task_queue_.dequeue();
  if (!task.has_value()) {
    return;
  }

  task.value()();
}

void ThreadPool::RunTimerCallback(PTP_TIMER timer) {
  DelayedTaskInfo* task_info = delayed_task_map_.get(timer);
  if (task_info == nullptr) {
    return;
  }

  DWORD thread_id = GetCurrentThreadId();
  task_info->thread_id = thread_id;

  task_info->task();

  if (task_info->is_canceled) {
    delayed_task_map_.erase(timer);
    return;
  }

  if (task_info->period == absl::ZeroDuration()) {
    task_info->is_done = true;
  }
}

bool ThreadPool::CancelDelayedTask(uint64_t delayed_task_id) {
  absl::MutexLock lock(mutex_);

  if (is_shut_down_) {
    LOG(WARNING)
        << __func__
        << ": Ignore to cancel delayed task because thread pool is shut down.";
    return false;
  }

  PTP_TIMER timer = reinterpret_cast<PTP_TIMER>(delayed_task_id);
  VLOG(1) << __func__ << ": Start to cancel timer(" << timer << ").";
  DelayedTaskInfo* task_info = delayed_task_map_.get(timer);

  if (task_info == nullptr) {
    return true;
  }

  BOOL is_canceled = SetThreadpoolTimerEx(/*pti=*/timer, /*pftDueTime=*/nullptr,
                                          /*msPeriod=*/0, /*msWindowLength=*/0);
  bool clean_timer = false;
  if (!is_canceled) {
    DWORD thread_id = GetCurrentThreadId();
    if (thread_id == task_info->thread_id) {
      // The timer is cancelled in the callback.
      VLOG(1) << __func__ << ": The timer is cancelled in the same thread."
              << task_info->thread_id;
      // The task_info is deleted upon completion of the running timer callback.
      // Developers must ensure proper resource management, considering access
      // by the callback and other threads.
      task_info->is_canceled = true;
    } else {
      WaitForThreadpoolTimerCallbacks(/*pti=*/timer,
                                      /*fCancelPendingCallbacks=*/TRUE);
      clean_timer = true;
    }
  }

  CloseThreadpoolTimer(/*pti=*/timer);
  if (is_canceled || clean_timer) {
    delayed_task_map_.erase(timer);
  }

  VLOG(1) << __func__ << ": Completed to cancel timer(" << timer << ").";
  return true;
}

void ThreadPool::TaskQueue::enqueue(Runnable task) {
  absl::MutexLock lock(mutex_);
  queue_.push(std::move(task));
}

std::optional<Runnable> ThreadPool::TaskQueue::dequeue() {
  absl::MutexLock lock(mutex_);
  if (queue_.empty()) {
    return std::nullopt;
  }

  Runnable value = std::move(queue_.front());
  queue_.pop();
  return std::move(value);
}

void ThreadPool::TaskQueue::clear() {
  absl::MutexLock lock(mutex_);
  queue_ = {};
}

void ThreadPool::DelayedTaskMap::put(
    PTP_TIMER timer, std::unique_ptr<DelayedTaskInfo> task_info) {
  absl::MutexLock lock(mutex_);
  task_map_[timer] = std::move(task_info);
}

ThreadPool::DelayedTaskInfo* ThreadPool::DelayedTaskMap::get(PTP_TIMER timer) {
  absl::MutexLock lock(mutex_);
  auto it = task_map_.find(timer);
  if (it == task_map_.end()) {
    return nullptr;
  }

  return it->second.get();
}

void ThreadPool::DelayedTaskMap::erase(PTP_TIMER timer) {
  absl::MutexLock lock(mutex_);
  task_map_.erase(timer);
}

void ThreadPool::DelayedTaskMap::clean_completed_tasks() {
  absl::MutexLock lock(mutex_);
  for (auto it = task_map_.begin(); it != task_map_.end();) {
    if (it->second->is_done) {
      CloseThreadpoolTimer(it->first);
      task_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void ThreadPool::DelayedTaskMap::clear() {
  absl::MutexLock lock(mutex_);
  task_map_.clear();
}

}  // namespace windows
}  // namespace nearby
