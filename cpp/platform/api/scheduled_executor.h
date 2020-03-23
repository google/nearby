#ifndef PLATFORM_API_SCHEDULED_EXECUTOR_H_
#define PLATFORM_API_SCHEDULED_EXECUTOR_H_

#include <cstdint>

#include "platform/api/executor.h"
#include "platform/cancelable.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public Executor {
 public:
  virtual ~ScheduledExecutor() {}

  virtual Ptr<Cancelable> schedule(Ptr<Runnable> runnable,
                                   std::int64_t delay_millis) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SCHEDULED_EXECUTOR_H_
