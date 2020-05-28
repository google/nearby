#include "platform/impl/shared/posix_lock.h"

namespace location {
namespace nearby {

PosixLock::PosixLock() : attr_(), mutex_() {
  pthread_mutexattr_init(&attr_);
  pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&mutex_, &attr_);
}

PosixLock::~PosixLock() {
  pthread_mutex_destroy(&mutex_);

  pthread_mutexattr_destroy(&attr_);
}

void PosixLock::lock() { pthread_mutex_lock(&mutex_); }

void PosixLock::unlock() { pthread_mutex_unlock(&mutex_); }

}  // namespace nearby
}  // namespace location
