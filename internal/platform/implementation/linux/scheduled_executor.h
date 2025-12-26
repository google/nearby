//
// Created by root on 10/8/25.
//

#ifndef WORKSPACE_SCHEDULED_EXECUTOR_H
#define WORKSPACE_SCHEDULED_EXECUTOR_H

#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/runnable.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/linux/multi_thread_executor.h"
#include <atomic>
#include <thread>
#include <utility>
#include "absl/time/time.h"

namespace nearby
{
  namespace linux
  {
    // Minimal ScheduledExecutor implementation for linux.
    class ScheduledExecutor : public api::ScheduledExecutor
    {
    public:
      ~ScheduledExecutor() override = default;
      ScheduledExecutor() : shutdown_(false), executor_(1) {}

      // Schedule a runnable to run after `duration`. Returns a Cancelable which
      // can be used to cancel the scheduled task before it runs.
      std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                           absl::Duration duration) override
      {
        class ScheduledCancelable : public api::Cancelable {
         public:
          enum Status { kNotRun, kExecuted, kCanceled };
          ScheduledCancelable() : status_(kNotRun) {}
          bool Cancel() override {
            Status expected = kNotRun;
            return status_.compare_exchange_strong(expected, kCanceled);
          }
          [[nodiscard]] bool IsCanceled() const { return status_.load() == kCanceled; }
          [[nodiscard]] bool MarkExecuted() {
            Status expected = kNotRun;
            return status_.compare_exchange_strong(expected, kExecuted);
          }

         private:
          std::atomic<Status> status_;
        };

        auto cancelable = std::make_shared<ScheduledCancelable>();
        if (shutdown_.load()) return cancelable;

        // Move runnable into the thread task.
        Runnable task = [this, cancelable, runnable = std::move(runnable)]() mutable {
          if (shutdown_.load()) return;
          if (cancelable->IsCanceled()) return;
          if (!cancelable->MarkExecuted()) return;
          // Use executor_ to run the actual runnable.
          executor_.Execute(std::move(runnable));
        };

        // Spawn a detached thread that sleeps for the duration then runs the task
        // through executor_. Using a detached thread is simple and sufficient for
        // a minimal implementation.
        std::thread([d = duration, t = std::move(task), cancelable, this]() mutable {
          if (absl::ToInt64Nanoseconds(d) > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(
                absl::ToInt64Nanoseconds(d)));
          }
          if (shutdown_.load()) return;
          if (cancelable->IsCanceled()) return;
          if (t) t();
        }).detach();

        return cancelable;
      };

      void Execute(Runnable&& runnable) override {
        if (shutdown_.load()) return;
        executor_.Execute(std::move(runnable));
      }

      void Shutdown() override {
        if (!shutdown_.exchange(true)) {
          executor_.Shutdown();
        }
      }

    private:
      std::atomic<bool> shutdown_;
      // Reuse the multi-thread executor implementation for running tasks.
      linux::MultiThreadExecutor executor_;
    };
  }
}
#endif //WORKSPACE_SCHEDULED_EXECUTOR_H
