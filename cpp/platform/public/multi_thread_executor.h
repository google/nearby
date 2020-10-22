#ifndef PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_

#include "platform/api/platform.h"
#include "platform/public/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newFixedThreadPool-int-
class MultiThreadExecutor final : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  explicit MultiThreadExecutor(int max_parallelism)
      : SubmittableExecutor(
            Platform::CreateMultiThreadExecutor(max_parallelism)) {}
  MultiThreadExecutor(MultiThreadExecutor&&) = default;
  MultiThreadExecutor& operator=(MultiThreadExecutor&&) = default;
  ~MultiThreadExecutor() override = default;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_
