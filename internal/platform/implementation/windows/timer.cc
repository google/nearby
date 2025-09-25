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
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

Timer::~Timer() { Stop(); }

bool Timer::Create(int delay, int interval,
                   absl::AnyInvocable<void()> callback) {
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
}

bool Timer::Stop() {
  absl::MutexLock lock(&mutex_);
  if (cancelable_task_ == nullptr) {
    return true;
  }

  bool result = cancelable_task_->Cancel();
  cancelable_task_ = nullptr;
  return result;
}

}  // namespace windows
}  // namespace nearby
