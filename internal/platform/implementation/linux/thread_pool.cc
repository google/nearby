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

#include "internal/platform/implementation/linux/thread_pool.h"

#include <queue>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux {

std::unique_ptr<ThreadPool> ThreadPool::Create(int max_pool_size) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Create thread pool with maximum size("
                       << max_pool_size << ").";

  if (max_pool_size <= 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Maximum pool size must be positive integer value.";
    return nullptr;
  }

  std::unique_ptr<std::vector<std::thread>> thread_pool = std::make_unique<std::vector<std::thread>>();

  // Sets thread pool maximum value.
  thread_pool->resize(max_pool_size);

  return absl::WrapUnique(
      new ThreadPool(thread_pool, max_pool_size));
}

ThreadPool::ThreadPool(std::unique_ptr<std::vector<std::thread>> &thread_pool, int max_pool_size)
    : thread_pool_(std::move(thread_pool)),
      max_pool_size_(max_pool_size) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Thread pool(" << this
                       << ") is created with size:" << max_pool_size_;

  for (int size = 0; size < max_pool_size_; size++) {
    thread_pool_->at(size) = std::thread([this]() {
      while (!tasks_.empty()) {
        RunNextTask();
        // Possibly don't need but here to prevent 100% usage for loop
        sleep(300);
      } 
    });
  }
}

ThreadPool::~ThreadPool() {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Thread pool(" << this
                       << ") is released.";

  ShutDown();
}

bool ThreadPool::Run(Runnable task) {
  absl::MutexLock lock(&mutex_);

  if (thread_pool_->size() == max_pool_size_) {
    return false;
  }

  tasks_.push(std::move(task));
  NEARBY_LOGS(VERBOSE) << __func__ << ": Scheduled to run task("
                       << &tasks_.back() << ").";
  return true;
}

void ThreadPool::ShutDown() {
  absl::MutexLock lock(&mutex_);

  if (!thread_pool_->empty()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Request to shutdown thread pool with " << thread_pool_->size() << " tasks not finished(" << this << ").";
  }

  NEARBY_LOGS(VERBOSE) << __func__ << ": Shutdown thread pool(" << this << ").";
  if (thread_pool_ == nullptr) {
    NEARBY_LOGS(WARNING) << __func__ << ": Shutdown on closed thread pool("
                         << this << ").";
    return;
  }

  thread_pool_.reset();
}

void ThreadPool::RunNextTask() {
  Runnable task = nullptr;

  {
    absl::MutexLock lock(&mutex_);

    if (!thread_pool_) {
      return;
    }
    if (!tasks_.empty()) {
      NEARBY_LOGS(VERBOSE) << __func__ << ": Run task(" << &tasks_.front()
                           << ").";

      task = std::move(tasks_.front());
      tasks_.pop();
    }
  }
  if (task == nullptr) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Tried to run task in an empty thread pool.";
    return;
  }

  task();
}

}  // namespace linux
}  // namespace nearby
