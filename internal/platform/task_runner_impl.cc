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

#include "internal/platform/task_runner_impl.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer_impl.h"

namespace nearby {

TaskRunnerImpl::TaskRunnerImpl(uint32_t runner_count) {
  if (runner_count == 1) {
    executor_ = std::make_unique<::nearby::SingleThreadExecutor>();
  } else {
    executor_ = std::make_unique<::nearby::MultiThreadExecutor>(runner_count);
  }
}

TaskRunnerImpl::~TaskRunnerImpl() {
  {
    absl::MutexLock lock(&mutex_);
    timers_map_.clear();
  }
  executor_->Shutdown();
}

bool TaskRunnerImpl::PostTask(absl::AnyInvocable<void()> task) {
  if (task) {
    // Because of cannot get the executor status from platform API, just returns
    // true after calling the Execute method.
    executor_->Execute(std::move(task));
  }

  return true;
}

bool TaskRunnerImpl::PostDelayedTask(absl::Duration delay,
                                     absl::AnyInvocable<void()> task) {
  if (!task) {
    return true;
  }

  absl::MutexLock lock(&mutex_);
  uint64_t id = GenerateId();
  std::unique_ptr<Timer> timer = std::make_unique<TimerImpl>();
  if (timer->Start(absl::ToInt64Milliseconds(delay), 0,
                   [this, id, task = std::move(task)]() mutable {
                     PostTask(std::move(task));
                     // We can't destroy the timer directly from the timer
                     // callback.
                     absl::MutexLock lock(&mutex_);
                     auto timer = timers_map_.extract(id);
                     PostTask([timer = std::move(timer)]() {});
                   })) {
    timers_map_.emplace(id, std::move(timer));
    return true;
  }

  return false;
}

uint64_t TaskRunnerImpl::GenerateId() { return nearby::RandData<uint64_t>(); }

}  // namespace nearby
