#ifndef PLATFORM_IMPL_DEFAULT_DEFAULT_LOCK_H_
#define PLATFORM_IMPL_DEFAULT_DEFAULT_LOCK_H_

#include <pthread.h>

#include "platform/api/lock.h"

namespace location {
namespace nearby {

class DefaultLock : public Lock {
 public:
  DefaultLock();
  ~DefaultLock() override;

  void lock() override;
  void unlock() override;

 private:
  friend class DefaultConditionVariable;

  pthread_mutexattr_t attr_;
  pthread_mutex_t mutex_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_DEFAULT_DEFAULT_LOCK_H_
