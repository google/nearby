#ifndef PLATFORM_V2_API_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_V2_API_SUBMITTABLE_EXECUTOR_H_

#include <functional>
#include <memory>

#include "platform_v2/api/executor.h"
#include "platform_v2/api/future.h"
#include "platform_v2/base/runnable.h"

namespace location {
namespace nearby {
namespace api {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(std::function<void()>) method.
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override = default;

  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  virtual bool DoSubmit(Runnable&& wrapped_callable) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_SUBMITTABLE_EXECUTOR_H_
