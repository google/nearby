#ifndef PLATFORM_API_CONDITION_VARIABLE_H_
#define PLATFORM_API_CONDITION_VARIABLE_H_

#include "platform/exception.h"

namespace location {
namespace nearby {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable {
 public:
  virtual ~ConditionVariable() {}

  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#notify--
  virtual void notify() = 0;
  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#wait--
  virtual Exception::Value wait() = 0;  // throws Exception::INTERRUPTED
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_CONDITION_VARIABLE_H_
