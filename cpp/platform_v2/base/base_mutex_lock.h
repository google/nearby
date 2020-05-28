#ifndef PLATFORM_V2_BASE_BASE_MUTEX_LOCK_H_
#define PLATFORM_V2_BASE_BASE_MUTEX_LOCK_H_

#include "platform_v2/api/mutex.h"
#include "absl/base/thread_annotations.h"

namespace location {
namespace nearby {

// An RAII mechanism to acquire a Lock over a block of code.
class ABSL_SCOPED_LOCKABLE BaseMutexLock final {
 public:
  explicit BaseMutexLock(api::Mutex* mutex) ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->Lock();
  }
  ~BaseMutexLock() ABSL_UNLOCK_FUNCTION() { mutex_->Unlock(); }

 private:
  api::Mutex* mutex_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_BASE_BASE_MUTEX_LOCK_H_
