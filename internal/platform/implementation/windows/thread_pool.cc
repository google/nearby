// Copyright 2021 Google LLC
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

#include <queue>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

VOID CALLBACK WorkCallback(PTP_CALLBACK_INSTANCE instance, PVOID parameter,
                           PTP_WORK work) {
  // Instance is not used in thread pool.
  UNREFERENCED_PARAMETER(instance);

  ThreadPool* thread_pool = reinterpret_cast<ThreadPool*>(parameter);
  thread_pool->RunNextTask();
  CloseThreadpoolWork(work);
}

std::unique_ptr<ThreadPool> ThreadPool::Create(int max_pool_size) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Create thread pool with maximum size("
                       << max_pool_size << ").";

  PTP_POOL thread_pool = nullptr;
  TP_CALLBACK_ENVIRON thread_pool_environ;
  InitializeThreadpoolEnvironment(&thread_pool_environ);

  if (max_pool_size <= 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Maximum pool size must be positive integer value.";
    return nullptr;
  }

  thread_pool = CreateThreadpool(NULL);
  if (thread_pool == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": failed to create thread pool. LastError: "
                       << GetLastError();
    return nullptr;
  }

  // Sets thread pool maximum value. In order to release all threads,
  // will keep at least one thread.
  SetThreadpoolThreadMaximum(thread_pool, max_pool_size);
  if (!SetThreadpoolThreadMinimum(thread_pool, 1)) {
    NEARBY_LOGS(ERROR)
        << __func__ << ": failed to set minimum thread pool size. LastError: "
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
  NEARBY_LOGS(VERBOSE) << __func__ << ": Thread pool(" << this
                       << ") is created.";
}

ThreadPool::~ThreadPool() {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Thread pool(" << this
                       << ") is released.";

  ShutDown();
}

bool ThreadPool::Run(Runnable task) {
  if (thread_pool_ == nullptr) {
    return false;
  }

  absl::MutexLock lock(&mutex_);
  PTP_WORK work;
  tasks_.push(std::move(task));
  NEARBY_LOGS(VERBOSE) << __func__ << ": Scheduled to run task("
                       << &tasks_.back() << ").";

  work = CreateThreadpoolWork(WorkCallback, this, &thread_pool_environ_);
  if (work == nullptr) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": failed to create thread pool work. LastError: "
                       << GetLastError();
    return false;
  }

  //
  // Submit the work to the pool. Because this was a pre-allocated
  // work item (using CreateThreadpoolWork), it is guaranteed to execute.
  //
  SubmitThreadpoolWork(work);
  return true;
}

void ThreadPool::ShutDown() {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Shutdown thread pool(" << this << ").";
  if (thread_pool_ == nullptr) {
    NEARBY_LOGS(WARNING) << __func__ << ": Shutdown on closed thread pool("
                         << this << ").";
    return;
  }

  CloseThreadpool(thread_pool_);
  thread_pool_ = nullptr;
}

void ThreadPool::RunNextTask() {
  if (thread_pool_ == nullptr) {
    return;
  }

  Runnable task = nullptr;
  {
    absl::MutexLock lock(&mutex_);
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

}  // namespace windows
}  // namespace nearby
