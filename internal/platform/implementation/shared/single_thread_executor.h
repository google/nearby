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

#ifndef PLATFORM_IMPL_SHARED_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_SHARED_SINGLE_THREAD_EXECUTOR_H_

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <utility>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/shared/multi_thread_executor.h"
#include "internal/platform/implementation/single_thread_executor.h"
#include "internal/platform/runnable.h"
#include "nisaba/port/thread_pool.h"

namespace nearby::shared {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
class SingleThreadExecutor final : public api::SingleThreadExecutor {
 public:
  SingleThreadExecutor()
      : executor_(1,
                  ThreadPool::Options{.force_eager_thread_creation = true}) {
    absl::Notification ready;
    executor_.Execute([this, &ready]() {
      thread_id_.store(GetPosixThreadId(), std::memory_order_release);
      ready.Notify();
    });
    ready.WaitForNotification();
  }
  ~SingleThreadExecutor() override { DoShutdown(); }

  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;

  bool Execute(Runnable&& runnable) override {
    if (!runnable || shutting_down_.load(std::memory_order_acquire)) {
      return false;
    }
    return executor_.Execute(std::move(runnable));
  }

  bool DelayedExecute(absl::Duration delay, Runnable&& runnable) override {
    if (!runnable || shutting_down_.load(std::memory_order_acquire) ||
        executor_.InShutdown()) {
      return false;
    }
    int64_t delay_ms = std::clamp<int64_t>(absl::ToInt64Milliseconds(delay), 0,
                                           std::numeric_limits<int>::max());
    if (delay_ms == 0) {
      return Execute(std::move(runnable));
    }
    return ScheduleAfter(absl::Milliseconds(delay_ms), std::move(runnable));
  }

  void Shutdown() override { DoShutdown(); }

  bool IsRunningOnExecutor() const override {
    return thread_id_.load(std::memory_order_acquire) == GetPosixThreadId();
  }

  bool ScheduleAfter(absl::Duration delay, Runnable&& runnable) {
    if (!runnable || shutting_down_.load(std::memory_order_acquire)) {
      return false;
    }
    return executor_.ScheduleAfter(
        delay, [this, runnable = std::move(runnable)]() mutable {
          if (shutting_down_.load(std::memory_order_acquire)) {
            return;
          }
          runnable();
        });
  }

  bool InShutdown() const {
    return shutting_down_.load(std::memory_order_acquire) ||
           executor_.InShutdown();
  }

 private:
  static pid_t GetPosixThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
  }

  void DoShutdown() {
    shutting_down_.store(true, std::memory_order_release);
    executor_.Shutdown();
    thread_id_.store(0, std::memory_order_release);
  }

  std::atomic<bool> shutting_down_{false};
  std::atomic<pid_t> thread_id_{0};
  MultiThreadExecutor executor_;
};

}  // namespace nearby::shared

#endif  // PLATFORM_IMPL_SHARED_SINGLE_THREAD_EXECUTOR_H_
