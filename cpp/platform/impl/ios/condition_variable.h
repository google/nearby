#ifndef PLATFORM_IMPL_IOS_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_IOS_CONDITION_VARIABLE_H_

#include "platform/api/condition_variable.h"
#include "platform/base/exception.h"
#include "platform/impl/ios/mutex.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace ios {

class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(ios::Mutex* mutex) : mutex_(&mutex->mutex_) {}
  ~ConditionVariable() override = default;

  Exception Wait() override {
    cond_var_.Wait(mutex_);
    return {Exception::kSuccess};
  }
  Exception Wait(absl::Duration timeout) override {
    cond_var_.WaitWithTimeout(mutex_, timeout);
    return {Exception::kSuccess};
  }
  void Notify() override { cond_var_.SignalAll(); }

 private:
  absl::Mutex* mutex_;
  absl::CondVar cond_var_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_CONDITION_VARIABLE_H_
