#include "platform/cancelable_alarm.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {

template <typename Platform>
CancelableAlarm<Platform>::CancelableAlarm(
    const string &name, Ptr<Runnable> runnable, std::int64_t delay_millis,
    Ptr<typename Platform::ScheduledExecutorType> scheduled_executor)
    : name_(name),
      lock_(Platform::createLock()),
      cancelable_(scheduled_executor->schedule(runnable, delay_millis)) {}

template <typename Platform>
CancelableAlarm<Platform>::~CancelableAlarm() {
  cancelable_.destroy();
}

template <typename Platform>
bool CancelableAlarm<Platform>::cancel() {
  Synchronized s(lock_.get());

  if (cancelable_.isNull()) {
    // TODO(tracyzhou): Add logging
    return false;
  }

  bool canceled = cancelable_->cancel();
  // TODO(tracyzhou): Add logging
  cancelable_.destroy();
  return canceled;
}

}  // namespace nearby
}  // namespace location
