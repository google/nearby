#ifndef PLATFORM_IMPL_DEFAULT_DEFAULT_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_DEFAULT_DEFAULT_CONDITION_VARIABLE_H_

#include <pthread.h>

#include "platform/api/condition_variable.h"
#include "platform/impl/default/default_lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class DefaultConditionVariable : public ConditionVariable {
 public:
  explicit DefaultConditionVariable(Ptr<DefaultLock> lock);
  ~DefaultConditionVariable() override;

  void notify() override;
  Exception::Value wait() override;

 private:
  Ptr<DefaultLock> lock_;
  pthread_condattr_t attr_;
  pthread_cond_t cond_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_DEFAULT_DEFAULT_CONDITION_VARIABLE_H_
