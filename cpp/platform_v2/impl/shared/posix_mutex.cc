#include "platform_v2/impl/shared/posix_mutex.h"

namespace location {
namespace nearby {
namespace posix {

Mutex::Mutex() : attr_(), mutex_() {
  pthread_mutexattr_init(&attr_);
  pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&mutex_, &attr_);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(&mutex_);

  pthread_mutexattr_destroy(&attr_);
}

void Mutex::Lock() { pthread_mutex_lock(&mutex_); }

void Mutex::Unlock() { pthread_mutex_unlock(&mutex_); }

}  // namespace posix
}  // namespace nearby
}  // namespace location
