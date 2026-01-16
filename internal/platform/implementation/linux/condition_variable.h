#ifndef PLATFORM_IMPL_LINUX_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_LINUX_CONDITION_VARIABLE_H_

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/linux/mutex.h"
#include "internal/platform/implementation/mutex.h"

namespace nearby {
namespace linux {
class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(api::Mutex *mutex)
      : mutex_(&(static_cast<linux::Mutex*>(mutex)->GetMutex())) {}
  ~ConditionVariable() = default;

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
  absl::Mutex *mutex_;
  absl::CondVar cond_var_;
};
}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_CONDITION_VARIABLE_H_
