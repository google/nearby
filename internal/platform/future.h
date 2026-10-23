// Copyright 2020 Google LLC
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

#ifndef PLATFORM_PUBLIC_FUTURE_H_
#define PLATFORM_PUBLIC_FUTURE_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

template <typename T>
class Future final {
 public:
  // Sets the value of the Future.
  bool Set(T value) {
    MutexLock lock(&state_->mutex);
    if (!state_->done) {
      state_->value = ExceptionOr<T>(std::move(value));
      state_->done = true;
      state_->completed.Notify();
      return true;
    }
    return false;
  }

  // Sets the exception of the Future.
  bool SetException(Exception exception) {
    MutexLock lock(&state_->mutex);
    return SetExceptionLocked(exception);
  }

  ExceptionOr<T> Get() {
    MutexLock lock(&state_->mutex);
    while (!state_->done) {
      state_->completed.Wait();
    }
    return state_->value;
  }

  // Gets the value of the Future, timing out after the specified duration.
  ExceptionOr<T> Get(absl::Duration timeout) {
    MutexLock lock(&state_->mutex);
    while (!state_->done) {
      absl::Time start_time = SystemClock::ElapsedRealtime();
      if (state_->completed.Wait(timeout).Raised(Exception::kInterrupted)) {
        SetExceptionLocked({Exception::kInterrupted});
        break;
      }
      absl::Duration spent = SystemClock::ElapsedRealtime() - start_time;
      if (spent < timeout) {
        timeout -= spent;
      } else if (!state_->done) {
        SetExceptionLocked({Exception::kTimeout});
        break;
      }
    }
    return state_->value;
  }

  // Returns true if the Future has been set.
  bool IsSet() const {
    MutexLock lock(&state_->mutex);
    return state_->done;
  }

 private:
  struct FutureState {
    mutable Mutex mutex;
    ConditionVariable completed{&mutex};
    bool done ABSL_GUARDED_BY(mutex) = {false};
    ExceptionOr<T> value ABSL_GUARDED_BY(mutex) =
        ExceptionOr<T>(Exception::kFailed);
  };

  bool SetExceptionLocked(Exception exception) {
    if (!state_->done) {
      state_->value = ExceptionOr<T>(exception.value != Exception::kSuccess
                                         ? exception
                                         : Exception{Exception::kFailed});
      state_->done = true;
      state_->completed.Notify();
    }
    return true;
  }

  std::shared_ptr<FutureState> state_ = std::make_shared<FutureState>();
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_FUTURE_H_
