#ifndef PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_
#define PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "platform/api/platform.h"
#include "platform/api/scheduled_executor.h"
#include "platform/base/runnable.h"
#include "platform/public/cancelable.h"
#include "platform/public/cancellable_task.h"
#include "platform/public/mutex.h"
#include "platform/public/mutex_lock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor final {
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
  void Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_) impl_->Execute(std::move(runnable));
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    DoShutdown();
  }

  int GetTid(int index) const {
    MutexLock lock(&mutex_);
    return impl_->GetTid(index);
  }
  int Tid() const { return GetTid(0); }

  Cancelable Schedule(Runnable&& runnable, absl::Duration duration)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    if (impl_) {
      auto task = std::make_shared<CancellableTask>(std::move(runnable));
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
}  // namespace location

#endif  // PLATFORM_PUBLIC_SCHEDULED_EXECUTOR_H_
