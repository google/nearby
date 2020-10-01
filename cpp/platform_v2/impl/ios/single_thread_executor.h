#ifndef PLATFORM_V2_IMPL_IOS_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_V2_IMPL_IOS_SINGLE_THREAD_EXECUTOR_H_

#include "platform_v2/impl/ios/multi_thread_executor.h"

namespace location {
namespace nearby {
namespace ios {

class SingleThreadExecutor final : public MultiThreadExecutor {
 public:
  SingleThreadExecutor() : MultiThreadExecutor(1) {}
  ~SingleThreadExecutor() override = default;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_IOS_SINGLE_THREAD_EXECUTOR_H_
