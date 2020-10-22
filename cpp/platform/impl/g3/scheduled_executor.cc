#include "platform/impl/g3/scheduled_executor.h"

#include <atomic>
#include <memory>

#include "platform/api/cancelable.h"
#include "platform/base/runnable.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace g3 {

namespace {

class ScheduledCancelable : public api::Cancelable {
 public:
  bool Cancel() override {
    Status expected = kNotRun;
    while (expected == kNotRun) {
      if (status_.compare_exchange_strong(expected, kCanceled)) {
        return true;
      }
    }
    return false;
  }
  bool MarkExecuted() {
    Status expected = kNotRun;
    while (expected == kNotRun) {
      if (status_.compare_exchange_strong(expected, kExecuted)) {
        return true;
      }
    }
    return false;
  }

 private:
  enum Status {
    kNotRun,
    kExecuted,
    kCanceled,
  };
  std::atomic<Status> status_ = kNotRun;
};

}  // namespace

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration delay) {
  auto scheduled_cancelable = std::make_shared<ScheduledCancelable>();
  if (executor_.InShutdown()) {
    return scheduled_cancelable;
  }
  executor_.ScheduleAfter(
      delay, [this, scheduled_cancelable, runnable(std::move(runnable))]() {
        if (!executor_.InShutdown() && scheduled_cancelable->MarkExecuted()) {
          runnable();
        }
      });
  return scheduled_cancelable;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
