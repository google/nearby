#ifndef PLATFORM_IMPL_G3_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_G3_SINGLE_THREAD_EXECUTOR_H_

#include "platform/impl/g3/multi_thread_executor.h"

namespace location {
namespace nearby {
namespace g3 {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
class SingleThreadExecutor final : public MultiThreadExecutor {
 public:
  SingleThreadExecutor() : MultiThreadExecutor(1) {}
  ~SingleThreadExecutor() override = default;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_SINGLE_THREAD_EXECUTOR_H_
