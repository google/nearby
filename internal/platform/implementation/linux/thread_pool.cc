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

#include <atomic>
#include <queue>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/thread_pool.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux {
ThreadPool::ThreadPool(size_t max_pool_size)
    : max_pool_size_(max_pool_size), shut_down_(false) {
  threads_.reserve(max_pool_size);
  Start();
}

ThreadPool::~ThreadPool() { ShutDown(); }

bool ThreadPool::Start() {
  shut_down_.store(false, std::memory_order_acquire);

  absl::MutexLock l(&mutex_);
  if (!threads_.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << "thread pool is already active";
    return false;
  }

  auto runner = [&]() {
    while (true) {
      if (shut_down_) {
        return;
      }

      auto task = NextTask();

      if (task == nullptr) {
        NEARBY_LOGS(WARNING) << __func__ << ": Tried to run a null task.";
        continue;
      }
      task();
    }
  };

  NEARBY_LOGS(INFO) << __func__ << ": Starting thread pool with "
                    << max_pool_size_ << " threads";

  for (size_t i = 0; i < max_pool_size_; i++) {
    threads_.emplace_back(runner);
  }

  return true;
}

bool ThreadPool::Run(Runnable &&task) {
  if (shut_down_) {
    NEARBY_LOGS(ERROR) << __func__ << "thread pool has shut down";
    return false;
  }

  absl::MutexLock l(&mutex_);
  if (threads_.empty()) {
    NEARBY_LOGS(ERROR) << __func__ << ": thread pool is not active";
    return false;
  }

  tasks_.push(std::move(task));
  return true;
}

void ThreadPool::ShutDown() {
  shut_down_.store(true, std::memory_order_acquire);

  NEARBY_LOGS(INFO)
      << __func__ << ": asked to shut down, waiting for active threads to stop";

  {
    absl::ReaderMutexLock l(&mutex_);
    for (auto &thread : threads_) {
      thread.join();
    }
  }

  absl::MutexLock l(&mutex_);
  threads_.clear();
  NEARBY_LOGS(INFO) << __func__ << ": shut down thread pool";
}

Runnable ThreadPool::NextTask() {
  Runnable task;
  auto task_available = [&]() {
    mutex_.AssertReaderHeld();
    return !tasks_.empty();
  };

  {
    absl::MutexLock l(&mutex_, absl::Condition(&task_available));

    task = std::move(tasks_.front());
    tasks_.pop();
  }

  return task;
}

}  // namespace linux
}  // namespace nearby
