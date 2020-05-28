#ifndef PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_

#include <pthread.h>

#include "platform/api/condition_variable.h"
#include "platform/impl/shared/posix_lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class PosixConditionVariable : public ConditionVariable {
 public:
  explicit PosixConditionVariable(Ptr<PosixLock> lock);
  ~PosixConditionVariable() override;

  void notify() override;
  Exception::Value wait() override;

 private:
  Ptr<PosixLock> lock_;
  pthread_condattr_t attr_;
  pthread_cond_t cond_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
