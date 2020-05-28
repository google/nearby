#include "platform/cancelable_alarm.h"

#include "platform/api/platform.h"
#include "platform/api/scheduled_executor.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {

namespace {
using Platform = platform::ImplementationPlatform;
}

CancelableAlarm::CancelableAlarm(const std::string &name,
                                 Ptr<Runnable> runnable,
                                 std::int64_t delay_millis,
                                 Ptr<ScheduledExecutor> scheduled_executor)
    : name_(name),
      lock_(Platform::createLock()),
      cancelable_(scheduled_executor->schedule(runnable, delay_millis)) {}

CancelableAlarm::~CancelableAlarm() { cancelable_.destroy(); }

bool CancelableAlarm::cancel() {
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
