#ifndef PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_
#define PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_

#include "platform/api/condition_variable.h"
#include "platform/api/lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Provides obvious portable implementations of a subset of the hooks specified
// within //platform/api/.
//
// It's highly recommended that custom Platform implementations delegate to
// these methods unless there's a very good reason not to.
class DefaultPlatform {
 public:
  static Ptr<Lock> createLock();

  static Ptr<ConditionVariable> createConditionVariable(Ptr<Lock> lock);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_
