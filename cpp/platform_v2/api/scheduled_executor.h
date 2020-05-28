#ifndef PLATFORM_V2_API_SCHEDULED_EXECUTOR_H_
#define PLATFORM_V2_API_SCHEDULED_EXECUTOR_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "platform_v2/api/cancelable.h"
#include "platform_v2/api/executor.h"
#include "platform_v2/base/runnable.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace api {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public Executor {
 public:
  ~ScheduledExecutor() override = default;
  // Cancelable is kept both in the executor context, and in the caller context.
  // We want Cancelable to live until both caller and executor are done with it.
  // Exclusive ownership model does not work for this case;
  // using std:shared_ptr<> instead if std::unique_ptr<>.
  virtual std::shared_ptr<Cancelable> Schedule(Runnable&& runnable,
                                               absl::Duration duration) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_SCHEDULED_EXECUTOR_H_
