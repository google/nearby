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

#include <memory>
#include <queue>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

VOID CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID parameter,
                           PTP_WORK work) {
  // Instance is not used in thread pools.
  UNREFERENCED_PARAMETER(instance);

  ThreadPool* thread_pool = reinterpret_cast<ThreadPool*>(parameter);
  thread_pool->RunNextTask();
  CloseThreadpoolWork(work);
}

std::unique_ptr<ThreadPool> ThreadPool::Create(int max_pool_size) {
  PTP_POOL thread_pool = nullptr;
  TP_CALLBACK_ENVIRON thread_pool_environ;
  InitializeThreadpoolEnvironment(&thread_pool_environ);

  if (max_pool_size <= 0) {
    LOG(ERROR) << __func__
               << ": Maximum pool size must be positive integer value.";
    return nullptr;
  }

  thread_pool = CreateThreadpool(NULL);
  if (thread_pool == nullptr) {
    LOG(ERROR) << __func__ << ": failed to create thread pool. LastError: "
               << GetLastError();
    return nullptr;
  }

  // Sets thread pool maximum value. In order to release all threads,
  // it will keep at least one thread.
  SetThreadpoolThreadMaximum(thread_pool, max_pool_size);
  if (!SetThreadpoolThreadMinimum(thread_pool, 1)) {
    LOG(ERROR) << __func__
               << ": failed to set minimum thread pool size. LastError: "
               << GetLastError();
    CloseThreadpool(thread_pool);
    return nullptr;
  }

  //
  // Associate the callback environment with our thread pool.
  //
  SetThreadpoolCallbackPool(&thread_pool_environ, thread_pool);

  return absl::WrapUnique(
      new ThreadPool(thread_pool, thread_pool_environ, max_pool_size));
}

ThreadPool::ThreadPool(PTP_POOL thread_pool,
                       TP_CALLBACK_ENVIRON thread_pool_environ,
                       int max_pool_size)
    : thread_pool_(thread_pool),
      thread_pool_environ_(thread_pool_environ),
      max_pool_size_(max_pool_size) {
  VLOG(1) << __func__ << ": Thread pool(" << this
          << ") is created with size:" << max_pool_size_;
}

ThreadPool::~ThreadPool() {
  VLOG(1) << __func__ << ": Thread pool(" << this << ") is released.";

  if (thread_pool_ == nullptr) {
    return;
  }

  ShutDown();
}

bool ThreadPool::Run(Runnable task) {
  absl::MutexLock lock(&mutex_);

  if (thread_pool_ == nullptr) {
    return false;
  }

  if (shutdown_latch_ != nullptr) {
    LOG(WARNING) << __func__ << ": Thread pool is in shutting down.";
    return false;
  }

  PTP_WORK work;
  tasks_.push(std::move(task));
  VLOG(1) << __func__ << ": Scheduled to run task(" << &tasks_.back() << ").";

  work = CreateThreadpoolWork(WorkCallback, this, &thread_pool_environ_);
  if (work == nullptr) {
    LOG(ERROR) << __func__ << ": failed to create thread pool work. LastError: "
               << GetLastError();
    return false;
  }

  ++running_tasks_count_;

  //
  // Submit the work to the pool. Because this was a pre-allocated
  // work item (using CreateThreadpoolWork), it is guaranteed to execute.
  //
  SubmitThreadpoolWork(work);
  return true;
}

void ThreadPool::ShutDown() {
  {
    absl::MutexLock lock(&mutex_);

    if (thread_pool_ == nullptr) {
      LOG(WARNING) << __func__ << ": Shutdown on closed thread pool(" << this
                   << ").";
      return;
    }

    if (running_tasks_count_ == 0) {
      CloseThreadpool(thread_pool_);
      thread_pool_ = nullptr;
      VLOG(1) << __func__ << ": Thread pool(" << this << ") is shut down.";
      return;
    }

    if (shutdown_latch_ != nullptr) {
      VLOG(1) << __func__ << ": Thread pool(" << this
              << ") is already in shutting down.";
      return;
    }

    VLOG(1) << __func__ << ": Thread pool(" << this << ") is shutting down.";

    shutdown_latch_ = std::make_unique<shared::CountDownLatch>(1);
  }

  // Wait for all tasks to complete.
  shutdown_latch_->Await();

  {
    absl::MutexLock lock(&mutex_);
    CloseThreadpool(thread_pool_);
    thread_pool_ = nullptr;
    VLOG(1) << __func__ << ": Thread pool(" << this << ") is shut down.";
  }
}

void ThreadPool::RunNextTask() {
  Runnable task = nullptr;

  {
    absl::MutexLock lock(&mutex_);

    if (thread_pool_ == nullptr) {
      return;
    }
    if (!tasks_.empty()) {
      VLOG(1) << __func__ << ": Run task(" << &tasks_.front() << ").";

      task = std::move(tasks_.front());
      tasks_.pop();

      if (task == nullptr) {
        LOG(WARNING) << __func__
                     << ": Tried to run task in an empty thread pool.";
        --running_tasks_count_;
        if (running_tasks_count_ == 0 && shutdown_latch_ != nullptr) {
          shutdown_latch_->CountDown();
        }
        return;
      }
    }
  }

  task();

  {
    absl::MutexLock lock(&mutex_);
    --running_tasks_count_;
    if (running_tasks_count_ == 0 && shutdown_latch_ != nullptr) {
      shutdown_latch_->CountDown();
    }
  }
}

}  // namespace windows
}  // namespace nearby
