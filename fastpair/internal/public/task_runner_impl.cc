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

#include "fastpair/internal/public/task_runner_impl.h"

#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

#include "absl/random/random.h"
#include "fastpair/internal/public/timer_impl.h"

namespace location {
namespace nearby {
namespace fastpair {

TaskRunnerImpl::TaskRunnerImpl(uint32_t runner_count) {
  executor_ =
      std::make_unique<::location::nearby::MultiThreadExecutor>(runner_count);
}

TaskRunnerImpl::~TaskRunnerImpl() = default;

bool TaskRunnerImpl::PostTask(std::function<void()> task) {
  if (!task) {
    return true;
  }

  // Because of cannot get the executor status from platform API, just returns
  // true after calling the Execute method.
  executor_->Execute(std::move(task));
  return true;
}

bool TaskRunnerImpl::PostDelayedTask(absl::Duration delay,
                                     std::function<void()> task) {
  if (!task) {
    return true;
  }

  absl::MutexLock lock(&mutex_);
  uint64_t id = GenerateId();

  std::unique_ptr<Timer> timer = std::make_unique<TimerImpl>();
  if (timer->Start(delay / absl::Milliseconds(1), 0,
                   [this, id, task = std::move(task)]() {
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

uint64_t TaskRunnerImpl::GenerateId() {
  absl::BitGen bitgen;
  return absl::Uniform(bitgen, 0u, std::numeric_limits<uint64_t>::max());
}

}  // namespace fastpair
}  // namespace nearby
}  // namespace location
