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

#ifndef PLATFORM_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_

#include <atomic>

#include "platform/api/submittable_executor.h"
#include "platform/impl/ios/count_down_latch.h"
#include "absl/time/clock.h"
#include "thread/threadpool.h"

namespace location {
namespace nearby {
namespace ios {

class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_parallelism)
      : thread_pool_(max_parallelism) {
    thread_pool_.StartWorkers();
  }
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

  int GetTid(int index) const override {
    const auto* thread = thread_pool_.thread(index);
    return thread ? *(int*)(thread->tid()) : 0;
  }

  void ScheduleAfter(absl::Duration delay, Runnable&& runnable) {
    if (shutdown_) return;
    thread_pool_.ScheduleAt(absl::Now() + delay, std::move(runnable));
  }
  bool InShutdown() const { return shutdown_; }

 private:
  void DoShutdown() {
    shutdown_ = true;
  }
  std::atomic_bool shutdown_ = false;
  ThreadPool thread_pool_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_
