// Copyright 2022 Google LLC
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

#include "internal/platform/single_thread_executor.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/timer.h"
#include "internal/platform/timer_impl.h"

namespace nearby {

SingleThreadExecutor::~SingleThreadExecutor() { DoShutdown(); }

void SingleThreadExecutor::Shutdown() { DoShutdown(); }

void SingleThreadExecutor::DoShutdown() {
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
  impl_->Shutdown();
}

bool SingleThreadExecutor::DelayedExecute(absl::Duration delay,
                                          absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(timer_state_->mutex);
  if (timer_state_->closed) {
    return false;
  }
  if (!task) {
    return true;
  }
  uint64_t timer_id = timer_state_->next_timer_id++;
  int delay_ms = static_cast<int>(std::clamp<int64_t>(
      absl::ToInt64Milliseconds(delay), 0, std::numeric_limits<int>::max()));
  absl_nonnull std::unique_ptr<Timer> timer = CreateTimer();
  if (timer->Start(delay_ms, 0,
                   [this, state = timer_state_, timer_id,
                    task = std::move(task)]() mutable {
                     // Hold a stack reference across MutexLock unlock in case
                     // Timer::Stop() destroys this callback closure early.
                     std::shared_ptr<TimerState> local_state = state;
                     std::unique_ptr<Timer> timer;
                     {
                       absl::MutexLock lock(local_state->mutex);
                       if (local_state->closed) {
                         return;
                       }
                       auto node = local_state->timers_map.extract(timer_id);
                       if (!node.empty()) {
                         timer = std::move(node.mapped());
                       }
                       ++local_state->active_operations;
                     }
                     Execute(std::move(task));
                     // We can't destroy the timer directly from the timer
                     // callback.
                     if (timer) {
                       DestroyOnExecutor(std::move(timer), this);
                     }
                     absl::MutexLock lock(local_state->mutex);
                     --local_state->active_operations;
                   })) {
    timer_state_->timers_map.emplace(timer_id, std::move(timer));
    return true;
  }

  return false;
}

absl_nonnull std::unique_ptr<Timer> SingleThreadExecutor::CreateTimer() {
  return std::make_unique<TimerImpl>();
}

}  // namespace nearby
