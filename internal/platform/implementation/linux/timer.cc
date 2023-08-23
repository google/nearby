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

#include "internal/platform/implementation/linux/timer.h"
#include "internal/platform/implementation/linux/timer_queue.h"

#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

Timer::~Timer() { Stop(); }

bool Timer::Create(int delay, int interval,
                   absl::AnyInvocable<void()> callback) {
  absl::MutexLock lock(&mutex_);

  if ((delay < 0) || (interval < 0)) {
    NEARBY_LOGS(WARNING) << "Delay and interval shouldn\'t be negative value.";
    return false;
  }

  if (timer_queue_handle_) {
    return false;
  }

  timer_queue_handle_ = TimerQueue::CreateTimerQueue();
  if (!timer_queue_handle_) {
    NEARBY_LOGS(ERROR) << "Failed to create timer queue.";
    return false;
  }

  delay_ = delay;
  interval_ = interval;
  callback_ = std::move(callback);

  absl::StatusOr<uint16_t> createStatus = timer_queue_handle_->CreateTimerQueueTimer(TimerRoutine,
                             &callback_, std::chrono::milliseconds(delay), std::chrono::milliseconds(interval), TimerQueue::WT_EXECUTEDEFAULT);

  if (!createStatus.ok()) {
    if (!timer_queue_handle_->DeleteTimerQueueEx(TimerQueue::CE_IMEDIATERETURN).ok()) {
      NEARBY_LOGS(ERROR) << "Failed to create timer in timer queue.";
    }
    delete timer_queue_handle_.release();
    return false;
  }
  handle_ = createStatus.value();
  return true;
}

bool Timer::Stop() {
  absl::MutexLock lock(&mutex_);

  if (!timer_queue_handle_) {
    return true;
  }

  absl::Status deleteStatus = timer_queue_handle_->DeleteTimerQueueTimer(handle_, TimerQueue::CE_IMEDIATERETURN);

  if (!deleteStatus.ok()) {
    NEARBY_LOGS(ERROR) << "Failed to delete timer from queue: " << deleteStatus.message();
  }

  handle_ = 0;

  deleteStatus = timer_queue_handle_->DeleteTimerQueueEx(TimerQueue::CE_IMEDIATERETURN);

  if (!deleteStatus.ok()) {
    NEARBY_LOGS(ERROR) << "Failed to delete timer queue: " << deleteStatus.message();
    return false;
  }

  timer_queue_handle_ = nullptr;
  return true;
}

bool Timer::FireNow() {
  absl::MutexLock lock(&mutex_);

  if (!timer_queue_handle_ || !callback_) {
    return false;
  }

  if (task_executor_ == nullptr) {
    task_executor_ = std::make_unique<SubmittableExecutor>();
  }

  if (task_executor_ == nullptr) {
    NEARBY_LOGS(ERROR)
        << "Failed to fire the task due to cannot create executor.";
    return false;
  }

  task_executor_->Execute([&]() { callback_(); });

  return true;
}

void Timer::TimerRoutine(void *lpParam) {
  absl::AnyInvocable<void()>* callback =
      reinterpret_cast<absl::AnyInvocable<void()>*>(lpParam);
  if (*callback != nullptr) {
    (*callback)();
  }
}

}  // namespace linux
}  // namespace nearby
