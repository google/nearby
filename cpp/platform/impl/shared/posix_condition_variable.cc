#include "platform/impl/shared/posix_condition_variable.h"

namespace location {
namespace nearby {
namespace posix {

ConditionVariable::ConditionVariable(Mutex* mutex)
    : mutex_(mutex), attr_(), cond_() {
  pthread_condattr_init(&attr_);

  pthread_cond_init(&cond_, &attr_);
}

ConditionVariable::~ConditionVariable() {
  pthread_cond_destroy(&cond_);

  pthread_condattr_destroy(&attr_);
}

void ConditionVariable::Notify() { pthread_cond_broadcast(&cond_); }

Exception ConditionVariable::Wait() {
  pthread_cond_wait(&cond_, &(mutex_->mutex_));

  return {Exception::kSuccess};
}

}  // namespace posix
}  // namespace nearby
}  // namespace location
