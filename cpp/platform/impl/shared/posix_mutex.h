#ifndef PLATFORM_IMPL_SHARED_POSIX_MUTEX_H_
#define PLATFORM_IMPL_SHARED_POSIX_MUTEX_H_

#include <pthread.h>

#include "platform/api/mutex.h"

namespace location {
namespace nearby {
namespace posix {

class ABSL_LOCKABLE Mutex : public api::Mutex {
 public:
  Mutex();
  ~Mutex() override;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override;
  void Unlock() ABSL_UNLOCK_FUNCTION() override;

 private:
  friend class ConditionVariable;

  pthread_mutexattr_t attr_;
  pthread_mutex_t mutex_;
};

}  // namespace posix
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_MUTEX_H_
