#ifndef PLATFORM_V2_PUBLIC_CONDITION_VARIABLE_H_
#define PLATFORM_V2_PUBLIC_CONDITION_VARIABLE_H_

#include "platform_v2/api/condition_variable.h"
#include "platform_v2/api/platform.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/public/mutex.h"

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

  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#notify--
  void Notify() { impl_->Notify(); }
  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#wait--
  Exception Wait() { return impl_->Wait(); }

 private:
  std::unique_ptr<api::ConditionVariable> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_CONDITION_VARIABLE_H_
