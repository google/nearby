#include "platform/impl/default/default_platform.h"

#include "platform/impl/default/default_condition_variable.h"
#include "platform/impl/default/default_lock.h"

namespace location {
namespace nearby {

Ptr<Lock> DefaultPlatform::createLock() { return MakePtr(new DefaultLock()); }

Ptr<ConditionVariable> DefaultPlatform::createConditionVariable(
    Ptr<Lock> lock) {
  return MakePtr(new DefaultConditionVariable(DowncastPtr<DefaultLock>(lock)));
}

}  // namespace nearby
}  // namespace location
