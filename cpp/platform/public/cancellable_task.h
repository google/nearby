#ifndef PLATFORM_PUBLIC_CANCELLABLE_TASK_H_
#define PLATFORM_PUBLIC_CANCELLABLE_TASK_H_

#include <utility>

#include "platform/base/feature_flags.h"
#include "platform/base/runnable.h"
#include "platform/public/atomic_boolean.h"
#include "platform/public/future.h"

namespace location {
namespace nearby {

/**
 * Runnable wrapper that allows one to wait for the task
 * to complete if it is already running.
 */
class CancellableTask {
 public:
  explicit CancellableTask(Runnable&& runnable)
      : runnable_{std::move(runnable)} {}

  /**
   * Try to cancel the task and wait until completion if the task is already
   * running.
   */
  void CancelAndWaitIfStarted() {
    if (started_or_cancelled_.Set(true)) {
      if (FeatureFlags::GetInstance()
              .GetFlags()
              .cancel_waits_for_running_tasks) {
        // task could still be running, wait until finish
        finished_.Get();
      }
    } else {
      // mark as finished to support multiple calls to this method
      finished_.Set(true);
    }
  }

  void operator()() {
    if (started_or_cancelled_.Set(true)) return;
    runnable_();
    finished_.Set(true);
  }

 private:
  AtomicBoolean started_or_cancelled_;
  Future<bool> finished_;
  Runnable runnable_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_CANCELLABLE_TASK_H_
