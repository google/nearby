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

#include <bits/types/struct_itimerspec.h>
#include <signal.h>
#include <time.h>
#include <cerrno>
#include <cstring>
#include <ctime>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/submittable_executor.h"
#include "internal/platform/implementation/linux/timer.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

static void timer_callback(union sigval val) {
  absl::AnyInvocable<void()> *callback =
      reinterpret_cast<absl::AnyInvocable<void()> *>(val.sival_ptr);
  if (*callback != nullptr) (*callback)();
}

Timer::~Timer() {
  absl::MutexLock l(&mutex_);
  if (timerid_.has_value())
    if (timer_delete(*timerid_) < 0) {
      NEARBY_LOGS(ERROR) << __func__ << ": Error deleting POSIX timer: "
                         << std::strerror(errno);
    }
}

bool Timer::Create(int delay, int interval,
                   absl::AnyInvocable<void()> callback) {
  if (delay < 0 || interval < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Delay and interval cannot be negative.";
    return false;
  }

  absl::MutexLock l(&mutex_);
  if (timerid_.has_value()) {
    NEARBY_LOGS(ERROR) << __func__
                       << "Timer has already been created and armed.";
    return false;
  }

  callback_ = std::move(callback);

  struct sigevent ev;
  ev.sigev_value.sival_ptr = &callback_;
  ev.sigev_notify_function = timer_callback;

  timer_t timerid;

  struct itimerspec spec;

  spec.it_value.tv_nsec = delay * 1000000;
  spec.it_value.tv_sec = 0;

  spec.it_interval.tv_nsec = interval * 1000000;
  spec.it_interval.tv_sec = 0;

  if (timer_create(CLOCK_MONOTONIC, &ev, &timerid) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error creating POSIX timer: "
                       << std::strerror(errno);
    return false;
  }

  if (timer_settime(&timerid, 0, &spec, nullptr) < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error arming POSIX timer: "
                       << std::strerror(errno);
    if (!timer_delete(&timerid)) {
      NEARBY_LOGS(ERROR) << __func__ << ": error deleting POSIX timer: "
                         << std::strerror(errno);
    }
    return false;
  }

  timerid_ = timerid;
  return true;
}

bool Timer::Stop() {
  absl::MutexLock l(&mutex_);
  if (!timerid_.has_value()) {
    NEARBY_LOGS(WARNING) << __func__ << ": no timer created";
    return true;
  }

  if (!timer_delete(&*timerid_)) {
    NEARBY_LOGS(ERROR) << __func__ << ": error deleting POSIX timer: "
                       << std::strerror(errno);
    return false;
  }

  timerid_.reset();

  return true;
}

bool Timer::FireNow() {
  absl::MutexLock lock(&mutex_);
  if (!timerid_.has_value()) {
    NEARBY_LOGS(ERROR) << __func__ << ": No timer has been created";
    return false;
  }
  if (callback_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No callback has been set";
    return false;
  }
  if (task_executor_ == nullptr) {
    task_executor_ = std::make_unique<SubmittableExecutor>();
  }

  task_executor_->Execute([&]() { callback_(); });

  return true;
}

}  // namespace linux
}  // namespace nearby
