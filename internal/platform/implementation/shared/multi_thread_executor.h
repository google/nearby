// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_SHARED_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_SHARED_MULTI_THREAD_EXECUTOR_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/runnable.h"
#include "nisaba/port/thread_pool.h"

namespace nearby::shared {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
class MultiThreadExecutor : public api::Executor {
 public:
  explicit MultiThreadExecutor(int max_parallelism)
      : thread_pool_(std::make_unique<ThreadPool>(max_parallelism)) {}
  ~MultiThreadExecutor() override { DoShutdown(); }

  bool Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) override {
    if (!runnable) {
      return false;
    }
    absl::MutexLock lock(mutex_);
    if (!thread_pool_) {
      return false;
    }
    thread_pool_->Schedule(std::move(runnable));
    return true;
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) override { DoShutdown(); }

  bool ScheduleAfter(absl::Duration delay, Runnable&& runnable)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    if (!runnable) {
      return false;
    }
    absl::MutexLock lock(mutex_);
    if (!thread_pool_) {
      return false;
    }
    thread_pool_->ScheduleAt(absl::Now() + delay, std::move(runnable));
    return true;
  }

  bool InShutdown() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    return thread_pool_ == nullptr;
  }

 private:
  void DoShutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    std::unique_ptr<ThreadPool> thread_pool;
    {
      absl::MutexLock lock(mutex_);
      thread_pool = std::move(thread_pool_);
    }
    // Destroy `thread_pool` outside `mutex_` so worker threads are joined
    // without deadlocking re-entrant `Execute()` calls.
  }

  mutable absl::Mutex mutex_;
  std::unique_ptr<ThreadPool> thread_pool_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::shared

#endif  // PLATFORM_IMPL_SHARED_MULTI_THREAD_EXECUTOR_H_
