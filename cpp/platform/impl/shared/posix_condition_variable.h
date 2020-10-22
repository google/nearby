#ifndef PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_

#include <pthread.h>

#include "platform/api/condition_variable.h"
#include "platform/impl/shared/posix_mutex.h"

namespace location {
namespace nearby {
namespace posix {

class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(Mutex* mutex);
  ~ConditionVariable() override;

  void Notify() override;
  Exception Wait() override;

 private:
  Mutex* mutex_;
  pthread_condattr_t attr_;
  pthread_cond_t cond_;
};

}  // namespace posix
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
