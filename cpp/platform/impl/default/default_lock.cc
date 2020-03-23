#include "platform/impl/default/default_lock.h"

namespace location {
namespace nearby {

DefaultLock::DefaultLock() : attr_(), mutex_() {
  pthread_mutexattr_init(&attr_);
  pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&mutex_, &attr_);
}

DefaultLock::~DefaultLock() {
  pthread_mutex_destroy(&mutex_);

  pthread_mutexattr_destroy(&attr_);
}

void DefaultLock::lock() { pthread_mutex_lock(&mutex_); }

void DefaultLock::unlock() { pthread_mutex_unlock(&mutex_); }

}  // namespace nearby
}  // namespace location
