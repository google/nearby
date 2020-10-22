#ifndef PLATFORM_PUBLIC_CONDITION_VARIABLE_H_
#define PLATFORM_PUBLIC_CONDITION_VARIABLE_H_

#include "platform/api/condition_variable.h"
#include "platform/api/platform.h"
#include "platform/base/exception.h"
#include "platform/public/mutex.h"

namespace location {
namespace nearby {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit ConditionVariable(Mutex* mutex)
      : impl_(Platform::CreateConditionVariable(mutex->impl_.get())) {}
  ConditionVariable(ConditionVariable&&) = default;
  ConditionVariable& operator=(ConditionVariable&&) = default;

  void Notify() { impl_->Notify(); }
  Exception Wait() { return impl_->Wait(); }
  Exception Wait(absl::Duration timeout) { return impl_->Wait(timeout); }

 private:
  std::unique_ptr<api::ConditionVariable> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_CONDITION_VARIABLE_H_
