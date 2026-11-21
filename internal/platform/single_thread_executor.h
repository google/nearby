// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_runnable.h"
#include "internal/platform/timer.h"

namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class ABSL_LOCKABLE SingleThreadExecutor : public api::Executor,
                                           public Lockable {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor() : impl_(Platform::CreateSingleThreadExecutor()) {}
  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;
  ~SingleThreadExecutor() override;

  virtual bool Execute(const std::string& name, Runnable&& runnable) {
    return impl_->Execute(MakeMonitoredRunnable(
        name, MakeThreadCheckRunnable(this, std::move(runnable))));
  }

  bool Execute(Runnable&& runnable) override {
    return impl_->Execute(MakeMonitoredRunnable(
        MakeThreadCheckRunnable(this, std::move(runnable))));
  }

  // Schedules `task` to execute on the worker thread after `delay` (clamped to
  // `[0, INT_MAX]` milliseconds). Returns `true` if `task` is empty or was
  // scheduled; returns `false` if the executor has been shut down or timer
  // creation fails. Pending delayed tasks that have not yet fired are cancelled
  // on `Shutdown()` or destruction.
  bool DelayedExecute(absl::Duration delay, absl::AnyInvocable<void()> task)
      ABSL_LOCKS_EXCLUDED(timer_state_->mutex);

  void Shutdown() override ABSL_LOCKS_EXCLUDED(timer_state_->mutex);

 protected:
  virtual absl_nonnull std::unique_ptr<Timer> CreateTimer();

 private:
  // Shared via `std::shared_ptr` so in-flight timer callbacks on platforms
  // whose `Timer::Stop()` does not block can safely check `closed` without
  // dereferencing a destroyed `SingleThreadExecutor`, while `DoShutdown()`
  // waits for `active_operations` to reach 0 before shutting down the worker.
  struct TimerState {
    static bool AllOperationsDone(TimerState* state)
        ABSL_SHARED_LOCKS_REQUIRED(state->mutex) {
      return state->active_operations == 0;
    }

    absl::Mutex mutex;
    absl::flat_hash_map<uint64_t, std::unique_ptr<Timer>> timers_map
        ABSL_GUARDED_BY(mutex);
    uint64_t next_timer_id ABSL_GUARDED_BY(mutex) = 0;
    int active_operations ABSL_GUARDED_BY(mutex) = 0;
    bool closed ABSL_GUARDED_BY(mutex) = false;
  };

  void DoShutdown() ABSL_LOCKS_EXCLUDED(timer_state_->mutex);

  const absl_nonnull std::unique_ptr<api::Executor> impl_;
  const std::shared_ptr<TimerState> timer_state_ =
      std::make_shared<TimerState>();
};

// Moves the object to `executor` and destroys it there.
// This pattern is useful when there are some tasks running on `executor` that
// hold a reference to `object`. The tasks will complete before `object` is
// destroyed.
template <typename T>
void DestroyOnExecutor(T object, SingleThreadExecutor* executor) {
  executor->Execute([object = std::move(object)] {});
}

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
