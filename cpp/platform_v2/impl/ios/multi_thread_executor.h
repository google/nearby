#ifndef PLATFORM_V2_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_V2_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_

#include <atomic>

#include "platform_v2/api/submittable_executor.h"
#include "platform_v2/impl/ios/count_down_latch.h"
#include "absl/time/clock.h"
#include "thread/threadpool.h"

namespace location {
namespace nearby {
namespace ios {

class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_parallelism)
      : thread_pool_(max_parallelism) {
    thread_pool_.StartWorkers();
  }
  void Execute(Runnable&& runnable) override {
    if (!shutdown_) {
      thread_pool_.Schedule(std::move(runnable));
    }
  }
  bool DoSubmit(Runnable&& runnable) override {
    if (shutdown_) return false;
    thread_pool_.Schedule(std::move(runnable));
    return true;
  }
  void Shutdown() override { DoShutdown(); }
  ~MultiThreadExecutor() override { DoShutdown(); }

  int GetTid(int index) const override {
    const auto* thread = thread_pool_.thread(index);
    return thread ? *(int*)(thread->tid()) : 0;
  }

  void ScheduleAfter(absl::Duration delay, Runnable&& runnable) {
    if (shutdown_) return;
    thread_pool_.ScheduleAt(absl::Now() + delay, std::move(runnable));
  }
  bool InShutdown() const { return shutdown_; }

 private:
  void DoShutdown() {
    shutdown_ = true;
  }
  std::atomic_bool shutdown_ = false;
  ThreadPool thread_pool_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_IOS_MULTI_THREAD_EXECUTOR_H_
