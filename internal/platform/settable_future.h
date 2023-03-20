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

#include <memory>
#include <utility>
#include <vector>

#include "internal/platform/condition_variable.h"
#include "internal/platform/implementation/listenable_future.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/system_clock.h"
#include "internal/platform/timer_impl.h"

namespace nearby {

template <typename T>
class SettableFuture : public api::SettableFuture<T> {
 public:
  using FutureCallback = typename api::ListenableFuture<T>::FutureCallback;
  SettableFuture() = default;

  // Creates a SettableFuture that fails with a kTimeout when `timeout` expires.
  explicit SettableFuture(absl::Duration timeout)
      : timer_(absl::make_unique<TimerImpl>()) {
    timer_->Start(absl::ToInt64Milliseconds(timeout), 0,
                  [this] { SetException({Exception::kTimeout}); });
  }
  ~SettableFuture() override = default;

  bool Set(T value) override {
    MutexLock lock(&mutex_);
    timer_.reset();
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

  void AddListener(FutureCallback callback, api::Executor* executor) override {
    MutexLock lock(&mutex_);
    if (done_) {
      executor->Execute(
          [value = GetLocked(), callback = std::move(callback)]() mutable {
            callback(std::move(value));
          });
    } else {
      listeners_.emplace_back(std::make_pair(executor, std::move(callback)));
    }
  }

  bool IsSet() const {
    MutexLock lock(&mutex_);
    return done_;
  }

  bool SetException(Exception exception) override {
    MutexLock lock(&mutex_);
    if (timer_) {
      timer_->Stop();
      // We can't destroy the timer from the timer.
      if (!exception.Raised(Exception::kTimeout)) {
        timer_.reset();
      }
    }
    return SetExceptionLocked(exception);
  }

  ExceptionOr<T> Get() override {
    MutexLock lock(&mutex_);
    while (!done_) {
      completed_.Wait();
    }
    return GetLocked();
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
    return GetLocked();
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

  ExceptionOr<T> GetLocked() {
    return exception_.value != Exception::kSuccess
               ? ExceptionOr<T>{exception_.value}
               : ExceptionOr<T>{value_};
  }

  void InvokeAllLocked() {
    for (auto& item : listeners_) {
      item.first->Execute(
          [value = GetLocked(), callback = std::move(item.second)]() mutable {
            callback(std::move(value));
          });
    }
    listeners_.clear();
  }

  mutable Mutex mutex_;
  ConditionVariable completed_{&mutex_};
  std::vector<std::pair<api::Executor*, FutureCallback>> listeners_;
  bool done_{false};
  T value_;
  Exception exception_{Exception::kFailed};
  std::unique_ptr<TimerImpl> timer_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SETTABLE_FUTURE_H_
