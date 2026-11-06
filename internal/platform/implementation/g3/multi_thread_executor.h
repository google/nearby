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

#ifndef PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_

#include <atomic>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/runnable.h"
#include "nisaba/port/thread_pool.h"

namespace nearby {
namespace g3 {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_parallelism)
#if defined(THIRD_PARTY_GLOOP_THREAD_THREADPOOL_H_) && \
    !THREAD_HAVE_ALTERNATE_THREAD_POOL
      // For 1-thread executors (including SingleThreadExecutor), spawn the
      // worker eagerly and disable gloop's 1-minute idle-worker GC so the
      // worker ThreadId stays constant for the lifetime of the executor.
      : thread_pool_(max_parallelism,
                     ThreadPool::Options{.force_eager_thread_creation =
                                             (max_parallelism == 1)}) {
  }
#else
      : thread_pool_(max_parallelism) {
    thread_pool_.StartWorkers();
  }
#endif
  void Execute(Runnable&& runnable) override {
    if (!shutdown_) {
      thread_pool_.Schedule(std::move(runnable));
    }
  }
  bool DoSubmit(Runnable&& runnable) override {
    if (shutdown_) return false;
    thread_pool_.Schedule(std::move(runnable));
    return true;
  }
  void Shutdown() override { DoShutdown(); }
  ~MultiThreadExecutor() override { DoShutdown(); }

  void ScheduleAfter(absl::Duration delay, Runnable&& runnable) {
    if (shutdown_) return;
    thread_pool_.ScheduleAt(absl::Now() + delay, std::move(runnable));
  }
  bool InShutdown() const { return shutdown_; }

 private:
  void DoShutdown() { shutdown_ = true; }
  std::atomic_bool shutdown_ = false;
  ThreadPool thread_pool_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_
