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
#include "internal/crypto/random.h"
#include "internal/platform/timer_impl.h"

namespace nearby {

TaskRunnerImpl::TaskRunnerImpl(uint32_t runner_count) {
  executor_ = std::make_unique<::nearby::MultiThreadExecutor>(runner_count);
}

TaskRunnerImpl::~TaskRunnerImpl() = default;

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
  if (timer->Start(delay / absl::Milliseconds(1), 0,
                   [this, id, task = std::move(task)]() mutable {
                     if (task) {
                       PostTask(std::move(task));
                     }
                     {
                       absl::MutexLock lock(&mutex_);
                       timers_map_.erase(id);
                     }
                   })) {
    timers_map_.emplace(id, std::move(timer));
    return true;
  }

  return false;
}

uint64_t TaskRunnerImpl::GenerateId() { return ::crypto::RandData<uint64_t>(); }

}  // namespace nearby
