#ifndef PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_
#define PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_

#include <functional>

#include "platform/api/executor.h"
#include "platform/api/future.h"
#include "platform/callable.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(std::function<void()>) method.
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override = default;

  template <typename T>
  Ptr<Future<T>> submit(Ptr<Callable<T>> callable);

 protected:
  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  virtual bool DoSubmit(std::function<void()> wrapped_callable) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_
