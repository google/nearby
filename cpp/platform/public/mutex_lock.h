#ifndef PLATFORM_PUBLIC_MUTEX_LOCK_H_
#define PLATFORM_PUBLIC_MUTEX_LOCK_H_

#include "platform/api/mutex.h"
#include "platform/public/mutex.h"
#include "absl/base/thread_annotations.h"

namespace location {
namespace nearby {

// An RAII mechanism to acquire a Lock over a block of code.
class ABSL_SCOPED_LOCKABLE MutexLock final {
 public:
  explicit MutexLock(Mutex* mutex) ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex->impl_.get()) {
    mutex_->Lock();
  }
  explicit MutexLock(RecursiveMutex* mutex) ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex->impl_.get()) {
    mutex_->Lock();
  }
  ~MutexLock() ABSL_UNLOCK_FUNCTION() { mutex_->Unlock(); }

 private:
  api::Mutex* mutex_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_MUTEX_LOCK_H_
