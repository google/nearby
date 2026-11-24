// Copyright 2026 Google LLC
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

#include "internal/test/test_single_thread_executor.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/single_thread_executor.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_timer.h"

namespace nearby {

class TestSingleThreadExecutor::Impl final : public api::SingleThreadExecutor {
 public:
  explicit Impl(FakeClock* absl_nonnull clock) : clock_(clock) {}
  ~Impl() override { Shutdown(); }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool Execute(Runnable&& runnable) override {
    return executor_->Execute(std::move(runnable));
  }

  bool DelayedExecute(absl::Duration delay, Runnable&& runnable) override {
    if (delay <= absl::ZeroDuration()) {
      return Execute(std::move(runnable));
    }
    absl::MutexLock lock(timer_state_->mutex);
    if (!runnable || timer_state_->closed) {
      return false;
    }
    uint64_t timer_id = timer_state_->next_timer_id++;
    int delay_ms = static_cast<int>(std::clamp<int64_t>(
        absl::ToInt64Milliseconds(delay), 1, std::numeric_limits<int>::max()));
    auto timer = std::make_unique<FakeTimer>(clock_);
    if (timer->Start(
            delay_ms, 0,
            [this, state = timer_state_, timer_id,
             runnable = std::move(runnable)]() mutable {
              // Hold a stack reference across `mutex` unlock in case
              // `FakeTimer::Stop()` destroys this callback closure
              // while it is executing.
              std::shared_ptr<TimerState> local_state = state;
              std::unique_ptr<Timer> completed_timer;
              {
                absl::MutexLock lock(local_state->mutex);
                if (local_state->closed) {
                  return;
                }
                auto node = local_state->timers_map.extract(timer_id);
                if (!node.empty()) {
                  completed_timer = std::move(node.mapped());
                }
                ++local_state->active_operations;
              }
              executor_->Execute(MakeMonitoredRunnable(std::move(runnable)));
              if (completed_timer) {
                executor_->Execute([timer = std::move(completed_timer)] {});
              }
              absl::MutexLock lock(local_state->mutex);
              --local_state->active_operations;
            })) {
      timer_state_->timers_map.emplace(timer_id, std::move(timer));
      return true;
    }
    return false;
  }

  void Shutdown() override {
    absl::flat_hash_map<uint64_t, std::unique_ptr<Timer>> timers;
    bool should_stop_timers = false;
    {
      absl::MutexLock lock(timer_state_->mutex);
      if (!timer_state_->closed) {
        timer_state_->closed = true;
        timers = std::move(timer_state_->timers_map);
        ++timer_state_->active_operations;
        should_stop_timers = true;
      }
    }
    if (should_stop_timers) {
      for (const auto& [unused_id, timer] : timers) {
        timer->Stop();
      }
    }
    {
      absl::MutexLock lock(timer_state_->mutex);
      if (should_stop_timers) {
        --timer_state_->active_operations;
      }
      timer_state_->mutex.Await(
          absl::Condition(&TimerState::AllOperationsDone, timer_state_.get()));
    }
    executor_->Shutdown();
  }

  bool IsRunningOnExecutor() const override {
    return executor_->IsRunningOnExecutor();
  }

 private:
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

  FakeClock* const clock_;
  const absl_nonnull std::unique_ptr<api::SingleThreadExecutor> executor_ =
      api::ImplementationPlatform::CreateSingleThreadExecutor();
  const std::shared_ptr<TimerState> timer_state_ =
      std::make_shared<TimerState>();
};

TestSingleThreadExecutor::TestSingleThreadExecutor(
    FakeClock* absl_nonnull clock)
    : SingleThreadExecutor(std::make_unique<Impl>(clock)) {}

TestSingleThreadExecutor::~TestSingleThreadExecutor() { Shutdown(); }

void TestSingleThreadExecutor::Shutdown() {
  closed_.store(true, std::memory_order_seq_cst);
  SingleThreadExecutor::Shutdown();
}

void TestSingleThreadExecutor::Sync() {
  if (closed_.load(std::memory_order_seq_cst)) {
    return;
  }
  absl::Notification notification;
  Execute([&notification] { notification.Notify(); });
  notification.WaitForNotification();
}

bool TestSingleThreadExecutor::SyncWithTimeout(absl::Duration timeout) {
  if (closed_.load(std::memory_order_seq_cst)) {
    return true;
  }
  auto notification = std::make_shared<absl::Notification>();
  Execute([notification] { notification->Notify(); });
  return notification->WaitForNotificationWithTimeout(timeout);
}

}  // namespace nearby
