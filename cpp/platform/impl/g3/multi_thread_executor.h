#ifndef PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_

#include <atomic>

#include "platform/api/submittable_executor.h"
#include "platform/impl/g3/count_down_latch.h"
#include "absl/time/clock.h"
#include "thread/threadpool.h"

namespace location {
namespace nearby {
namespace g3 {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
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
    return thread ? thread->tid() : 0;
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

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_MULTI_THREAD_EXECUTOR_H_
