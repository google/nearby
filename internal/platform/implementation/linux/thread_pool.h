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

#ifndef PLATFORM_IMPL_LINUX_THREAD_POOL_H_
#define PLATFORM_IMPL_LINUX_THREAD_POOL_H_

#include <atomic>
#include <memory>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux {

class ThreadPool {
 public:
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;
  explicit ThreadPool(size_t max_pool_size);
  ~ThreadPool();

  bool Start() ABSL_LOCKS_EXCLUDED(threads_mutex_);

  // Runs a task on thread pool. The result indicates whether the task is put
  // into the thread pool.
  bool Run(Runnable &&task) ABSL_LOCKS_EXCLUDED(tasks_mutex_);

  void ShutDown() ABSL_LOCKS_EXCLUDED(threads_mutex_);

 private:
  Runnable NextTask() ABSL_LOCKS_EXCLUDED(tasks_mutex_);

  size_t max_pool_size_;
  std::atomic_bool shut_down_;

  absl::Mutex threads_mutex_;
  std::vector<std::thread> threads_ ABSL_GUARDED_BY(threads_mutex_);
  absl::Mutex tasks_mutex_;
  std::queue<Runnable> tasks_ ABSL_GUARDED_BY(tasks_mutex_);
};
}  // namespace linux
}  // namespace nearby

#endif  //  PLATFORM_IMPL_LINUX_THREAD_POOL_H_
