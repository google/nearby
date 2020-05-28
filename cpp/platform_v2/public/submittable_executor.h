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
  Mutex mutex_;
  std::unique_ptr<api::SubmittableExecutor> ABSL_GUARDED_BY(mutex_) impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_SUBMITTABLE_EXECUTOR_H_
