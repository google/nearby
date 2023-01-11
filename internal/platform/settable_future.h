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

#ifndef PLATFORM_PUBLIC_SETTABLE_FUTURE_H_
#define PLATFORM_PUBLIC_SETTABLE_FUTURE_H_

#include <utility>
#include <vector>

#include "internal/platform/condition_variable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/system_clock.h"

namespace nearby {

template <typename T>
class SettableFuture : public api::SettableFuture<T> {
 public:
  SettableFuture() = default;
  ~SettableFuture() override = default;

  bool Set(T value) override {
    MutexLock lock(&mutex_);
    if (!done_) {
      value_ = std::move(value);
      done_ = true;
      exception_ = {Exception::kSuccess};
      completed_.Notify();
      InvokeAllLocked();
      return true;
    }
    return false;
  }

  void AddListener(Runnable runnable, api::Executor* executor) override {
    MutexLock lock(&mutex_);
    if (done_) {
      executor->Execute(std::move(runnable));
    } else {
      listeners_.emplace_back(std::make_pair(executor, std::move(runnable)));
    }
  }

  bool IsSet() const {
    MutexLock lock(&mutex_);
    return done_;
  }

  bool SetException(Exception exception) override {
    MutexLock lock(&mutex_);
    return SetExceptionLocked(exception);
  }

  ExceptionOr<T> Get() override {
    MutexLock lock(&mutex_);
    while (!done_) {
      completed_.Wait();
    }
    return exception_.value != Exception::kSuccess
               ? ExceptionOr<T>{exception_.value}
               : ExceptionOr<T>{value_};
  }

  ExceptionOr<T> Get(absl::Duration timeout) override {
    MutexLock lock(&mutex_);
    while (!done_) {
      absl::Time start_time = SystemClock::ElapsedRealtime();
      if (completed_.Wait(timeout).Raised(Exception::kInterrupted)) {
        SetExceptionLocked({Exception::kInterrupted});
        break;
      }
      absl::Duration spent = SystemClock::ElapsedRealtime() - start_time;
      if (spent < timeout) {
        timeout -= spent;
      } else if (!done_) {
        SetExceptionLocked({Exception::kTimeout});
        break;
      }
    }
    return exception_.value != Exception::kSuccess
               ? ExceptionOr<T>{exception_.value}
               : ExceptionOr<T>{value_};
  }

 private:
  bool SetExceptionLocked(Exception exception) {
    if (!done_) {
      exception_ = exception.value != Exception::kSuccess
                       ? exception
                       : Exception{Exception::kFailed};
      done_ = true;
      completed_.Notify();
      InvokeAllLocked();
    }
    return true;
  }

  void InvokeAllLocked() {
    for (auto& item : listeners_) {
      item.first->Execute(std::move(item.second));
    }
    listeners_.clear();
  }

  mutable Mutex mutex_;
  ConditionVariable completed_{&mutex_};
  std::vector<std::pair<api::Executor*, Runnable>> listeners_;
  bool done_{false};
  T value_;
  Exception exception_{Exception::kFailed};
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SETTABLE_FUTURE_H_
