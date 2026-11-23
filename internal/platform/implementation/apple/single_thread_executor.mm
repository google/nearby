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

#include "internal/platform/implementation/apple/single_thread_executor.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#import "internal/platform/implementation/apple/scheduled_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace apple {

SingleThreadExecutor::SingleThreadExecutor() {
  scheduled_executor_ = std::make_unique<ScheduledExecutor>(1);
}

SingleThreadExecutor::~SingleThreadExecutor() { Shutdown(); }

bool SingleThreadExecutor::Execute(Runnable&& runnable) {
  return scheduled_executor_->Execute(std::move(runnable));
}

bool SingleThreadExecutor::DelayedExecute(absl::Duration delay, Runnable&& runnable) {
  if (delay <= absl::ZeroDuration()) {
    return Execute(std::move(runnable));
  }
  if (!runnable || scheduled_executor_->IsShuttingDown()) {
    return false;
  }
  constexpr absl::Duration kMaxDelay = absl::Milliseconds(std::numeric_limits<int>::max());
  return scheduled_executor_->Schedule(std::move(runnable), std::min(delay, kMaxDelay)) != nullptr;
}

void SingleThreadExecutor::Shutdown() { scheduled_executor_->Shutdown(); }

bool SingleThreadExecutor::IsRunningOnExecutor() const {
  return scheduled_executor_->IsRunningOnExecutor();
}

}  // namespace apple
}  // namespace nearby
