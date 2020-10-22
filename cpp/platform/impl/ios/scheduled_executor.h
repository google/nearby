#ifndef PLATFORM_IMPL_IOS_SCHEDULED_EXECUTOR_H_
#define PLATFORM_IMPL_IOS_SCHEDULED_EXECUTOR_H_

#include <atomic>
#include <memory>

#include "platform/api/cancelable.h"
#include "platform/api/scheduled_executor.h"
#include "platform/base/runnable.h"
#include "platform/impl/ios/single_thread_executor.h"
#include "absl/time/clock.h"
#include "thread/threadpool.h"

namespace location {
namespace nearby {
namespace ios {

class ScheduledExecutor final : public api::ScheduledExecutor {
 public:
  ScheduledExecutor() = default;
  ~ScheduledExecutor() override {
    executor_.Shutdown();
  }

  void Execute(Runnable&& runnable) override {
    executor_.Execute(std::move(runnable));
  }
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration delay) override;
  void Shutdown() override { executor_.Shutdown(); }

  int GetTid(int index) const override {
    return executor_.GetTid(index);
  }
 private:
  SingleThreadExecutor executor_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_IOS_SCHEDULED_EXECUTOR_H_
