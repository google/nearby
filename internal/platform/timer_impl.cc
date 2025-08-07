// Copyright 2021 Google LLC
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

#include "internal/platform/timer_impl.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"

namespace nearby {

bool TimerImpl::Start(int delay, int period,
                      absl::AnyInvocable<void()> callback) {
  if (internal_timer_ != nullptr) {
    LOG(INFO) << "The timer is already running.";
    return false;
  }

  delay_ = delay;
  period_ = period;
  internal_timer_ = api::ImplementationPlatform::CreateTimer();
  if (!internal_timer_->Create(delay, period, std::move(callback))) {
    LOG(INFO) << "Failed to create timer.";
    internal_timer_ = nullptr;
    return false;
  }
  return true;
}

void TimerImpl::Stop() {
  if (internal_timer_ == nullptr) {
    return;
  }
  // Stop returns false if timer has already fired.  We can ignore that.
  internal_timer_->Stop();
  internal_timer_ = nullptr;
}

bool TimerImpl::IsRunning() { return (internal_timer_ != nullptr); }

}  // namespace nearby
