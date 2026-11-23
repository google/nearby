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

#include "internal/platform/implementation/windows/single_thread_executor.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "location/nearby/apps/better_together/windows/common/windows_executor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/runnable.h"

namespace nearby::windows {

SingleThreadExecutor::SingleThreadExecutor()
    : executor_(std::make_unique<WindowsExecutor>()),
      started_(executor_->Start()) {}

SingleThreadExecutor::~SingleThreadExecutor() { Shutdown(); }

bool SingleThreadExecutor::Execute(Runnable&& runnable) {
  if (!started_ || !runnable) {
    return false;
  }
  return executor_->Schedule(std::move(runnable));
}

bool SingleThreadExecutor::DelayedExecute(absl::Duration delay,
                                          Runnable&& runnable) {
  if (delay <= absl::ZeroDuration()) {
    return Execute(std::move(runnable));
  }
  if (!started_ || !runnable) {
    return false;
  }
  constexpr absl::Duration kMaxDelay =
      absl::Milliseconds((std::numeric_limits<int>::max)());
  return executor_->ScheduleAt(absl::Now() + std::min(delay, kMaxDelay),
                               std::move(runnable));
}

void SingleThreadExecutor::Shutdown() { executor_->Shutdown(); }

bool SingleThreadExecutor::IsRunningOnExecutor() const {
  return executor_->IsRunningOnExecutor();
}

}  // namespace nearby::windows
