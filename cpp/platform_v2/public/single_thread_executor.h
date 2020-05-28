#ifndef PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_

#include "platform_v2/public/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class SingleThreadExecutor final : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor()
      : SubmittableExecutor(Platform::CreateSingleThreadExecutor()) {}
  ~SingleThreadExecutor() override = default;
  SingleThreadExecutor(SingleThreadExecutor&&) = default;
  SingleThreadExecutor& operator=(SingleThreadExecutor&&) = default;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
