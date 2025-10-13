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

#ifndef PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_
#define PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/cancelable.h"
#include "internal/platform/cancellable_task.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_runnable.h"

namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ABSL_LOCKABLE ScheduledExecutor final : public Lockable {
 public:
  ScheduledExecutor()
      : impl_(api::ImplementationPlatform::CreateScheduledExecutor()) {}
  ScheduledExecutor(ScheduledExecutor&& other) { *this = std::move(other); }
  ~ScheduledExecutor() { DoShutdown(); }

  ScheduledExecutor& operator=(ScheduledExecutor&& other)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    {
      MutexLock other_lock(&other.mutex_);
      impl_ = std::move(other.impl_);
    }
    return *this;
  }
  void Execute(const std::string& name, Runnable&& runnable)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_)
      impl_->Execute(MonitoredRunnable(
          name, ThreadCheckRunnable(this, std::move(runnable))));
  }

  void Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_) impl_->Execute(ThreadCheckRunnable(this, std::move(runnable)));
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) { DoShutdown(); }

  Cancelable Schedule(Runnable&& runnable, absl::Duration duration)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_) {
      auto task = std::make_shared<CancellableTask>(
          ThreadCheckRunnable(this, std::move(runnable)));
      return Cancelable(task,
                        impl_->Schedule([task]() { (*task)(); }, duration));
    } else {
      return Cancelable();
    }
  }

  Cancelable ScheduleRepeatedly(Runnable&& runnable, absl::Duration duration)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    {
      MutexLock lock(&mutex_);
      if (!impl_) {
        return Cancelable();
      }
    }

    auto cancellable_task = std::make_shared<CancellableTask>(
        ThreadCheckRunnable(this, std::move(runnable)), /*is_repeated=*/true);

    auto repeated_task_handler =
        std::make_shared<RepeatedTask>(this, cancellable_task, duration);

    // Start the first execution.
    repeated_task_handler->Start();
    return Cancelable(cancellable_task, repeated_task_handler);
  }

 private:
  // This class encapsulates the state and re-scheduling logic for a single
  // repeated task. An instance is created for each call to
  // ScheduleRepeatedly.
  class RepeatedTask : public api::Cancelable,
                       public std::enable_shared_from_this<RepeatedTask> {
   public:
    RepeatedTask(ScheduledExecutor* executor,
                 std::shared_ptr<CancellableTask> cancellable_task,
                 absl::Duration delay)
        : executor_(executor),
          cancellable_task_(std::move(cancellable_task)),
          delay_(delay) {}

    // Starts the first execution of the task.
    void Start() {
      MutexLock lock(&executor_->mutex_);
      ScheduleNextUnderLock();
    }

    // Implementation of api::Cancelable. This cancels future executions.
    bool Cancel() override {
      if (cancelled_.Set(true)) {
        return true;  // Already cancelled
      }

      // Cancel the pending scheduled task, if any.
      MutexLock lock(&executor_->mutex_);
      if (pending_future_) {
        pending_future_->Cancel();
      }
      return true;
    }

   private:
    void Run() {
      if (cancelled_.Get()) {
        return;
      }

      (*cancellable_task_)();

      if (cancelled_.Get()) {
        return;
      }

      // Re-schedule the next execution.
      MutexLock lock(&executor_->mutex_);
      ScheduleNextUnderLock();
    }

    void ScheduleNextUnderLock()
        ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_->mutex_) {
      if (cancelled_.Get() || !executor_->impl_) {
        return;
      }
      // Schedule the next run, capturing a shared_ptr to ourself to keep
      // this object alive.
      pending_future_ = executor_->impl_->Schedule(
          [self = shared_from_this()]() { self->Run(); }, delay_);
    }

    ScheduledExecutor* const executor_;
    const std::shared_ptr<CancellableTask> cancellable_task_;
    const absl::Duration delay_;

    AtomicBoolean cancelled_{false};
    std::shared_ptr<api::Cancelable> pending_future_
        ABSL_GUARDED_BY(executor_->mutex_);
  };

  void DoShutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    std::unique_ptr<api::ScheduledExecutor> executor = ReleaseExecutor();
    if (executor) {
      executor->Shutdown();
    }
  }

  std::unique_ptr<api::ScheduledExecutor> ReleaseExecutor()
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return std::move(impl_);
  }

  mutable Mutex mutex_;
  std::unique_ptr<api::ScheduledExecutor> ABSL_GUARDED_BY(mutex_) impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_
