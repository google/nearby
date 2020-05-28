#ifndef PLATFORM_IMPL_SHARED_POSIX_LOCK_H_
#define PLATFORM_IMPL_SHARED_POSIX_LOCK_H_

#include <pthread.h>

#include "platform/api/lock.h"

namespace location {
namespace nearby {

class PosixLock : public Lock {
 public:
  PosixLock();
  ~PosixLock() override;

  void lock() override;
  void unlock() override;

 private:
  friend class PosixConditionVariable;

  pthread_mutexattr_t attr_;
  pthread_mutex_t mutex_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_LOCK_H_
