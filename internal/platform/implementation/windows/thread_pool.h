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

#include <memory>
#include <queue>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

class ThreadPool {
 public:
  virtual ~ThreadPool();
  static std::unique_ptr<ThreadPool> Create(int max_pool_size);

  // Runs a task on thread pool. The result indicates whether the task is put
  // into the thread pool.
  bool Run(Runnable task) ABSL_LOCKS_EXCLUDED(mutex_);

  // In Nearby platform, thread pool should make sure all queued tasks completed
  // in shut down.
  void ShutDown();

 private:
  ThreadPool(PTP_POOL thread_pool, TP_CALLBACK_ENVIRON thread_pool_environ,
             int max_pool_size);
  void RunNextTask();

  // Protects the access to tasks of the thread pool.
  mutable absl::Mutex mutex_;

  // The task queue of the thread pool. Thread pool will pick up task to run
  // when it is idle.
  std::queue<Runnable> tasks_ ABSL_GUARDED_BY(mutex_);

  // Keeps the pointer of the thread pool. It is created when constructing the
  // thread pool.
  PTP_POOL thread_pool_ ABSL_GUARDED_BY(mutex_) = nullptr;

  // Keeps the environment of the thread pool.
  TP_CALLBACK_ENVIRON thread_pool_environ_ ABSL_GUARDED_BY(mutex_);

  // The maximum thread count in the thread pool
  int max_pool_size_ ABSL_GUARDED_BY(mutex_) = 0;

  // Current running task count
  int running_tasks_count_ ABSL_GUARDED_BY(mutex_) = 0;

  // The latch is used to wait for running tasks
  std::unique_ptr<CountDownLatch> shutdown_latch_ = nullptr;

  friend VOID CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE instance,
                                    PVOID parameter, PTP_WORK work);
};

}  // namespace windows
}  // namespace nearby

#endif  //  PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
