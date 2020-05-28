#ifndef PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_

#include "platform/api/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class SingleThreadExecutor : public SubmittableExecutor {
 public:
  ~SingleThreadExecutor() override = default;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SINGLE_THREAD_EXECUTOR_H_
