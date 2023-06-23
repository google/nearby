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

#ifndef PLATFORM_IMPL_G3_TIMER_H_
#define PLATFORM_IMPL_G3_TIMER_H_

#include <atomic>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/g3/scheduled_executor.h"
#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace g3 {

class Timer : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override = default;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override {
    if (delay < 0 || interval < 0 || callback == nullptr) {
      return false;
    }
    interval_ = absl::Milliseconds(interval);
    callback_ = std::move(callback);
    is_stopped_ = false;
    return Schedule(absl::Milliseconds(delay));
  }

  bool Stop() override {
    is_stopped_ = true;
    absl::MutexLock lock(&mutex_);
    if (task_) {
      bool result = task_->Cancel();
      task_.reset();
      return result;
    }
    return false;
  }

  bool FireNow() override {
    if (is_stopped_) {
      return false;
    }
    callback_();
    return true;
  }

 private:
  bool Schedule(absl::Duration delay) {
    absl::MutexLock lock(&mutex_);
    task_ = executor_.Schedule([this]() { TriggerCallback(); }, delay);
    return true;
  }

  void TriggerCallback() {
    if (is_stopped_) return;
    // If interval is 0, timer is one shot.
    if (interval_ != absl::ZeroDuration()) {
      Schedule(interval_);
    }
    callback_();
  }

 private:
  absl::Mutex mutex_;
  absl::AnyInvocable<void()> callback_;
  std::atomic_bool is_stopped_;
  absl::Duration interval_;
  std::shared_ptr<api::Cancelable> task_ ABSL_GUARDED_BY(mutex_);
  ScheduledExecutor executor_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_TIMER_H_
