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

#ifndef PLATFORM_V2_PUBLIC_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_V2_PUBLIC_SUBMITTABLE_EXECUTOR_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

#include "platform_v2/api/executor.h"
#include "platform_v2/api/submittable_executor.h"
#include "platform_v2/base/callable.h"
#include "platform_v2/base/runnable.h"
#include "platform_v2/public/future.h"
#include "platform_v2/public/mutex.h"
#include "platform_v2/public/mutex_lock.h"

namespace location {
namespace nearby {

inline int GetCurrentTid() { return api::GetCurrentTid(); }

// Main interface to be used by platform as a base class for
// - MultiThreadExecutor
// - SingleThreadExecutor
class SubmittableExecutor : public api::SubmittableExecutor {
 public:
  ~SubmittableExecutor() override {
    MutexLock lock(&mutex_);
    DoShutdown();
  }
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
  void Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) override {
    MutexLock lock(&mutex_);
    if (impl_) impl_->Execute(std::move(runnable));
  }

  int GetTid(int index) const ABSL_LOCKS_EXCLUDED(mutex_) override {
    MutexLock lock(&mutex_);
    return impl_ ? impl_->GetTid(index) : 0;
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) override {
    MutexLock lock(&mutex_);
    DoShutdown();
  }

  // Submits a callable for execution.
  // When execution completes, return value is assigned to the passed future.
  // Future must outlive the whole execution chain.
  template <typename T>
  bool Submit(Callable<T>&& callable, Future<T>* future)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    bool submitted = DoSubmit([callable{std::move(callable)}, future]() {
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
  void DoShutdown() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (impl_) {
      impl_->Shutdown();
      impl_.reset();
    }
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
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_SUBMITTABLE_EXECUTOR_H_
