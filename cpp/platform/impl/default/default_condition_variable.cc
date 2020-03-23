#include "platform/impl/default/default_condition_variable.h"

namespace location {
namespace nearby {

DefaultConditionVariable::DefaultConditionVariable(Ptr<DefaultLock> lock)
    : lock_(lock), attr_(), cond_() {
  pthread_condattr_init(&attr_);

  pthread_cond_init(&cond_, &attr_);
}

DefaultConditionVariable::~DefaultConditionVariable() {
  pthread_cond_destroy(&cond_);

  pthread_condattr_destroy(&attr_);
}

void DefaultConditionVariable::notify() { pthread_cond_broadcast(&cond_); }

Exception::Value DefaultConditionVariable::wait() {
  pthread_cond_wait(&cond_, &(lock_->mutex_));

  return Exception::NONE;
}

}  // namespace nearby
}  // namespace location
