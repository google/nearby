// Copyright 2020-2023 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
#define PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

class ThreadPool {
 public:
  virtual ~ThreadPool();
  static std::unique_ptr<ThreadPool> Create(uint32_t max_pool_size);

  // Runs a task on thread pool. The result indicates whether the task is put
  // into the thread pool.
  bool Run(Runnable task) ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs a task on thread pool with a delay. A delayed task ID is returned if
  // it is put into the thread pool. The ID can be used to cancel the task.
  std::optional<uint64_t> Run(Runnable task, absl::Duration delay)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs a task on thread pool with a delay and period. A delayed task ID is
  // returned if it is put into the thread pool. The ID can be used to cancel
  // the task.
  std::optional<uint64_t> Run(Runnable task, absl::Duration delay,
                              absl::Duration period)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Cancels a delayed task. It will return after running callback is completed.
  bool CancelDelayedTask(uint64_t delayed_task_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // In Nearby platform, thread pool should make sure all queued tasks completed
  // in shut down.
  void ShutDown() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct DelayedTaskInfo {
    Runnable task;
    absl::Duration delay;
    absl::Duration period;
    // Thread id 0 is an invalid value in windows.
    std::atomic_uint32_t thread_id = 0;
    std::atomic_bool is_done = false;
    std::atomic_bool is_canceled = false;

    DelayedTaskInfo(Runnable task, absl::Duration delay,
                    absl::Duration period) {
      this->task = std::move(task);
      this->delay = delay;
      this->period = period;
    }

    DelayedTaskInfo(const DelayedTaskInfo&) = delete;
    DelayedTaskInfo& operator=(const DelayedTaskInfo&) = delete;
  };

  class TaskQueue {
   public:
    TaskQueue() = default;
    ~TaskQueue() = default;

    void enqueue(Runnable task) ABSL_LOCKS_EXCLUDED(mutex_);
    std::optional<Runnable> dequeue() ABSL_LOCKS_EXCLUDED(mutex_);
    void clear() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    absl::Mutex mutex_;
    std::queue<Runnable> queue_ ABSL_GUARDED_BY(mutex_);
  };

  class DelayedTaskMap {
   public:
    DelayedTaskMap() = default;
    ~DelayedTaskMap() = default;

    void put(PTP_TIMER timer, std::unique_ptr<DelayedTaskInfo> task_info)
        ABSL_LOCKS_EXCLUDED(mutex_);
    DelayedTaskInfo* get(PTP_TIMER timer) ABSL_LOCKS_EXCLUDED(mutex_);
    void erase(PTP_TIMER timer) ABSL_LOCKS_EXCLUDED(mutex_);
    void clean_completed_tasks() ABSL_LOCKS_EXCLUDED(mutex_);
    void clear() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    absl::Mutex mutex_;
    absl::flat_hash_map<PTP_TIMER, std::unique_ptr<DelayedTaskInfo>> task_map_
        ABSL_GUARDED_BY(mutex_);
  };

  ThreadPool(PTP_POOL thread_pool, TP_CALLBACK_ENVIRON thread_pool_environ,
             PTP_CLEANUP_GROUP cleanup_group, int max_pool_size);

  static VOID CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE instance,
                                    PVOID parameter, PTP_WORK work);

  static VOID CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE instance,
                                     PVOID context, PTP_TIMER Timer);

  void RunNextTask();
  void RunTimerCallback(PTP_TIMER timer);

  // Protects the access to methods of the thread pool.
  absl::Mutex mutex_;

  // Keeps the pointer of the thread pool. It is created when constructing the
  // thread pool.
  PTP_POOL thread_pool_ ABSL_GUARDED_BY(mutex_) = nullptr;

  // Keeps the environment of the thread pool.
  TP_CALLBACK_ENVIRON thread_pool_environ_ ABSL_GUARDED_BY(mutex_);

  // The cleanup group of the thread pool.
  PTP_CLEANUP_GROUP cleanup_group_ ABSL_GUARDED_BY(mutex_) = nullptr;

  // The maximum thread count in the thread pool
  int max_pool_size_ ABSL_GUARDED_BY(mutex_) = 0;

  std::atomic_bool is_shut_down_ = false;

  // The task queue of the thread pool. Thread pool will pick up task to run
  // when it is idle.
  TaskQueue task_queue_;

  // The delayed task map of the thread pool. Thread pool will pick up delayed
  // task to run when it is idle.
  DelayedTaskMap delayed_task_map_;
};

}  // namespace windows
}  // namespace nearby

#endif  //  PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
