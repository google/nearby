#include "platform/impl/shared/posix_condition_variable.h"

namespace location {
namespace nearby {

PosixConditionVariable::PosixConditionVariable(Ptr<PosixLock> lock)
    : lock_(lock), attr_(), cond_() {
  pthread_condattr_init(&attr_);

  pthread_cond_init(&cond_, &attr_);
}

PosixConditionVariable::~PosixConditionVariable() {
  pthread_cond_destroy(&cond_);

  pthread_condattr_destroy(&attr_);
}

void PosixConditionVariable::notify() { pthread_cond_broadcast(&cond_); }

Exception::Value PosixConditionVariable::wait() {
  pthread_cond_wait(&cond_, &(lock_->mutex_));

  return Exception::kSuccess;
}

}  // namespace nearby
}  // namespace location
