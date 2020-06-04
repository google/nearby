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

#ifndef PLATFORM_V2_IMPL_G3_SETTABLE_FUTURE_ANY_H_
#define PLATFORM_V2_IMPL_G3_SETTABLE_FUTURE_ANY_H_

#include <utility>

#include "platform_v2/api/platform.h"
#include "platform_v2/api/settable_future.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace g3 {

class SettableFutureAny : public api::SettableFuture<absl::any> {
 public:
  SettableFutureAny() = default;
  ~SettableFutureAny() override = default;

  bool Set(const absl::any& value) override {
    absl::MutexLock lock(&mutex_);
    if (!done_) {
      value_ = value;
      done_ = true;
      exception_ = {Exception::kSuccess};
      completed_.SignalAll();
    }
    return true;
  }

  bool Set(absl::any&& value) override {
    absl::MutexLock lock(&mutex_);
    if (!done_) {
      value_ = std::move(value);
      done_ = true;
      exception_ = {Exception::kSuccess};
      completed_.SignalAll();
    }
    return true;
  }

  bool SetException(Exception exception) override {
    absl::MutexLock lock(&mutex_);
    return SetExceptionLocked(exception);
  }

  void AddListener(Runnable runnable, api::Executor* executor) override {}

  ExceptionOr<std::any> Get() override {
    absl::MutexLock lock(&mutex_);
    while (!done_) {
      completed_.Wait(&mutex_);
    }
    return exception_.value != Exception::kSuccess
               ? ExceptionOr<std::any>{exception_.value}
               : ExceptionOr<std::any>{value_};
  }

  ExceptionOr<std::any> Get(absl::Duration timeout) override {
    absl::MutexLock lock(&mutex_);
    while (!done_) {
      absl::Time start_time = absl::Now();
      if (completed_.WaitWithTimeout(&mutex_, timeout)) {
        SetExceptionLocked({Exception::kTimeout});
        break;
      }
      absl::Duration spent = absl::Now() - start_time;
      if (spent < timeout) {
        timeout -= spent;
      } else if (!done_) {
        SetExceptionLocked({Exception::kTimeout});
        break;
      }
    }
    return exception_.value != Exception::kSuccess
               ? ExceptionOr<std::any>{exception_.value}
               : ExceptionOr<std::any>{value_};
  }

 private:
  bool SetExceptionLocked(Exception exception) {
    if (!done_) {
      exception_ = exception.value != Exception::kSuccess
                       ? exception
                       : Exception{Exception::kFailed};
      done_ = true;
      completed_.SignalAll();
    }
    return true;
  }

  absl::Mutex mutex_;
  absl::CondVar completed_;
  bool done_{false};
  absl::any value_;
  Exception exception_{Exception::kFailed};
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_SETTABLE_FUTURE_ANY_H_
