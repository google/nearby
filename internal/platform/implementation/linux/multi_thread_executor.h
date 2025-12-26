#ifndef PLATFORM_IMPL_LINUX_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_LINUX_MULTI_THREAD_EXECUTOR_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace linux {

class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_parallelism)
      : shutdown_(false) {
    for (int i = 0; i < max_parallelism; ++i) {
      workers_.emplace_back([this]() { WorkerLoop(); });
    }
  }

  ~MultiThreadExecutor() override {
    Shutdown();
    for (auto& worker : workers_) {
      if (worker.joinable()) worker.join();
    }
  }
  void Schedule(Runnable&& runnable, absl::Duration delay) {
    if (shutdown_) return;
    std::thread([this, runnable = std::move(runnable), delay]() mutable {
      std::this_thread::sleep_for(std::chrono::nanoseconds(
          absl::ToInt64Nanoseconds(delay))); // delaying execution
      DoSubmit(std::move(runnable));
    }).detach();
  }

  void Execute(Runnable&& runnable) override {
    DoSubmit(std::move(runnable));
  }

  bool DoSubmit(Runnable&& runnable) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (shutdown_) return false;
      tasks_.emplace(std::move(runnable));
    }
    cv_.notify_one();
    return true;
  }

  void Shutdown() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
    }
    cv_.notify_all();
  }

 private:
  void WorkerLoop() {
    while (true) {
      Runnable task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return shutdown_ || !tasks_.empty(); });
        if (shutdown_ && tasks_.empty()) return;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      if (task) task();
    }
  }

  std::vector<std::thread> workers_;
  std::queue<Runnable> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> shutdown_;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_MULTI_THREAD_EXECUTOR_H_
