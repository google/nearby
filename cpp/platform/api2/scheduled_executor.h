#ifndef PLATFORM_API2_SCHEDULED_EXECUTOR_H_
#define PLATFORM_API2_SCHEDULED_EXECUTOR_H_

#include <cstdint>
#include <memory>

#include "platform/api2/executor.h"
#include "platform/cancelable.h"
#include "platform/runnable.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public Executor {
 public:
  ~ScheduledExecutor() override = default;
  virtual std::unique_ptr<Cancelable> Schedule(
      std::unique_ptr<Runnable> runnable, absl::Duration duration) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SCHEDULED_EXECUTOR_H_
