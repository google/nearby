#ifndef PLATFORM_API2_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_API2_SINGLE_THREAD_EXECUTOR_H_

#include "platform/api2/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
template <typename ConcreteSubmittableExecutor>
class SingleThreadExecutor
    : public SubmittableExecutor<ConcreteSubmittableExecutor> {
 public:
  ~SingleThreadExecutor() override {}
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SINGLE_THREAD_EXECUTOR_H_
