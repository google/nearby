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

#include "internal/platform/implementation/windows/timer.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

Timer::Timer()
    : use_task_scheduler_(NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableTaskScheduler)) {}

Timer::~Timer() { Stop(); }

bool Timer::Create(int delay, int interval,
                   absl::AnyInvocable<void()> callback) {
  if (use_task_scheduler_) {
    absl::MutexLock lock(&mutex_);
    if ((delay < 0) || (interval < 0)) {
      LOG(WARNING) << "Delay and interval shouldn\'t be negative value.";
      return false;
    }

    if (cancelable_task_) {
      return false;
    }
    callback_ = std::move(callback);
    std::function<void()> internal_callback = [this]() {
      if (callback_ != nullptr) {
        callback_();
      }
    };
    cancelable_task_ = task_scheduler_.Schedule(std::move(internal_callback),
                                                absl::Milliseconds(delay),
                                                absl::Milliseconds(interval));
    return cancelable_task_ != nullptr;
  } else {
    absl::MutexLock lock(&mutex_);

    if ((delay < 0) || (interval < 0)) {
      LOG(WARNING) << "Delay and interval shouldn\'t be negative value.";
      return false;
    }

    if (timer_queue_handle_ != nullptr) {
      return false;
    }

    timer_queue_handle_ = CreateTimerQueue();
    if (timer_queue_handle_ == nullptr) {
      LOG(ERROR) << "Failed to create timer queue.";
      return false;
    }

    delay_ = delay;
    interval_ = interval;
    callback_ = std::move(callback);

    if (!CreateTimerQueueTimer(&handle_, timer_queue_handle_,
                               static_cast<WAITORTIMERCALLBACK>(TimerRoutine),
                               &callback_, delay, interval,
                               WT_EXECUTEDEFAULT)) {
      if (!DeleteTimerQueueEx(timer_queue_handle_, nullptr)) {
        LOG(ERROR) << "Failed to create timer in timer queue.";
      }
      timer_queue_handle_ = nullptr;
      return false;
    }

    return true;
  }
}

bool Timer::Stop() {
  if (use_task_scheduler_) {
    absl::MutexLock lock(&mutex_);
    if (cancelable_task_ == nullptr) {
      return true;
    }

    bool result = cancelable_task_->Cancel();
    cancelable_task_ = nullptr;
    return result;
  } else {
    absl::MutexLock lock(&mutex_);

    if (timer_queue_handle_ == nullptr) {
      return true;
    }

    if (!DeleteTimerQueueTimer(timer_queue_handle_, handle_, nullptr)) {
      if (GetLastError() != ERROR_IO_PENDING) {
        LOG(ERROR) << "Failed to delete timer from timer queue.";
        return false;
      }
    }

    handle_ = nullptr;

    if (!DeleteTimerQueueEx(timer_queue_handle_, nullptr)) {
      LOG(ERROR) << "Failed to delete timer queue.";
      return false;
    }

    timer_queue_handle_ = nullptr;
    return true;
  }
}

bool Timer::FireNow() {
  absl::MutexLock lock(&mutex_);

  if (!callback_) {
    LOG(ERROR) << "callback_ is empty";
    return false;
  }

  if (task_executor_ == nullptr) {
    task_executor_ = std::make_unique<SubmittableExecutor>();
  }

  if (task_executor_ == nullptr) {
    LOG(ERROR) << "Failed to fire the task due to cannot create executor.";
    return false;
  }

  task_executor_->Execute([&]() { callback_(); });

  return true;
}

void CALLBACK Timer::TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired) {
  absl::AnyInvocable<void()>* callback =
      reinterpret_cast<absl::AnyInvocable<void()>*>(lpParam);
  if (*callback != nullptr) {
    (*callback)();
  }
}

}  // namespace windows
}  // namespace nearby
