#ifndef PLATFORM_CANCELABLE_ALARM_H_
#define PLATFORM_CANCELABLE_ALARM_H_

#include <cstdint>

#include "platform/api/lock.h"
#include "platform/cancelable.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

/**
 * A cancelable alarm with a name. This is a simple wrapper around the logic
 * for posting a Runnable on a ScheduledExecutor and (possibly) later
 * canceling it.
 */
template <typename Platform>
class CancelableAlarm {
 public:
  CancelableAlarm(
      const string& name, Ptr<Runnable> runnable, std::int64_t delay_millis,
      Ptr<typename Platform::ScheduledExecutorType> scheduled_executor);
  ~CancelableAlarm();

  bool cancel();

 private:
  string name_;
  ScopedPtr<Ptr<Lock> > lock_;
  Ptr<Cancelable> cancelable_;
};

}  // namespace nearby
}  // namespace location

#include "platform/cancelable_alarm.cc"

#endif  // PLATFORM_CANCELABLE_ALARM_H_
