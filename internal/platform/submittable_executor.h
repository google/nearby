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

#ifndef PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "internal/platform/callable.h"
#include "internal/platform/future.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_callable.h"
#include "internal/platform/thread_check_runnable.h"

namespace nearby {

inline int GetCurrentTid() { return api::GetCurrentTid(); }

// Main interface to be used by platform as a base class for
// - MultiThreadExecutor
// - SingleThreadExecutor
class ABSL_LOCKABLE SubmittableExecutor : public api::SubmittableExecutor,
                                          public Lockable {
 public:
  ~SubmittableExecutor() override { DoShutdown(); }
  SubmittableExecutor(SubmittableExecutor&& other) { *this = std::move(other); }
  SubmittableExecutor& operator=(SubmittableExecutor&& other)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    {
      MutexLock other_lock(&other.mutex_);
      impl_ = std::move(other.impl_);
    }
    return *this;
  }
  virtual void Execute(const std::string& name, Runnable&& runnable)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_)
      impl_->Execute(MonitoredRunnable(
          name, ThreadCheckRunnable(this, std::move(runnable))));
  }

  void Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) override {
    MutexLock lock(&mutex_);
    if (impl_)
      impl_->Execute(
          MonitoredRunnable(ThreadCheckRunnable(this, std::move(runnable))));
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) override { DoShutdown(); }

  // Submits a callable for execution.
  // When execution completes, return value is assigned to the passed future.
  // Future must outlive the whole execution chain.
  template <typename T>
  bool Submit(Callable<T>&& callable, Future<T>* future)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    bool submitted =
        DoSubmit([callable = ThreadCheckCallable<T>(this, std::move(callable)),
                  future]() mutable {
          ExceptionOr<T> result = callable();
          if (result.ok()) {
            future->Set(result.result());
          } else {
            future->SetException({result.exception()});
          }
        });
    if (!submitted) {
      // complete immediately with kExecution exception value.
      future->SetException({Exception::kExecution});
    }
    return submitted;
  }

 protected:
  explicit SubmittableExecutor(std::unique_ptr<api::SubmittableExecutor> impl)
      : impl_(std::move(impl)) {}

 private:
  void DoShutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    std::unique_ptr<api::SubmittableExecutor> executor = ReleaseExecutor();
    if (executor) {
      executor->Shutdown();
    }
  }

  std::unique_ptr<api::SubmittableExecutor> ReleaseExecutor()
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return std::move(impl_);
  }

  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  bool DoSubmit(Runnable&& wrapped_callable)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) override {
    return impl_ ? impl_->DoSubmit(std::move(wrapped_callable)) : false;
  }
  mutable Mutex mutex_;
  std::unique_ptr<api::SubmittableExecutor> ABSL_GUARDED_BY(mutex_) impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_
