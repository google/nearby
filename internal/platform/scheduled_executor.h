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

#include <cstdint>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/cancelable.h"
#include "internal/platform/cancellable_task.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_callable.h"
#include "internal/platform/thread_check_runnable.h"

namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ABSL_LOCKABLE ScheduledExecutor final : public Lockable {
 public:
  using Platform = api::ImplementationPlatform;

  ScheduledExecutor() : impl_(Platform::CreateScheduledExecutor()) {}
  ScheduledExecutor(ScheduledExecutor&& other) { *this = std::move(other); }
  ~ScheduledExecutor() {
    MutexLock lock(&mutex_);
    DoShutdown();
  }

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

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    DoShutdown();
  }

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

 private:
  void DoShutdown() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (impl_) {
      impl_->Shutdown();
      impl_.reset();
    }
  }

  mutable Mutex mutex_;
  std::unique_ptr<api::ScheduledExecutor> ABSL_GUARDED_BY(mutex_) impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_
