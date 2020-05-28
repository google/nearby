#ifndef PLATFORM_API_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_API_MULTI_THREAD_EXECUTOR_H_

#include "platform/api/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newFixedThreadPool-int-
class MultiThreadExecutor : public SubmittableExecutor {
 public:
  ~MultiThreadExecutor() override = default;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_MULTI_THREAD_EXECUTOR_H_
