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

#ifndef PLATFORM_V2_IMPL_G3_COUNT_DOWN_LATCH_H_
#define PLATFORM_V2_IMPL_G3_COUNT_DOWN_LATCH_H_

#include "platform_v2/api/count_down_latch.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace g3 {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch final : public api::CountDownLatch {
 public:
  explicit CountDownLatch(int count) : count_(count) {}
  CountDownLatch(const CountDownLatch&) = delete;
  CountDownLatch& operator=(const CountDownLatch&) = delete;
  CountDownLatch(CountDownLatch&&) = delete;
  CountDownLatch& operator=(CountDownLatch&&) = delete;
  ExceptionOr<bool> Await(absl::Duration timeout) override {
    absl::MutexLock lock(&mutex_);
    absl::Time deadline = absl::Now() + timeout;
    while (count_ > 0) {
      if (cond_.WaitWithDeadline(&mutex_, deadline)) {
        return ExceptionOr<bool>(false);
      }
    }
    return ExceptionOr<bool>(true);
  }
  Exception Await() override {
    absl::MutexLock lock(&mutex_);
    while (count_ > 0) {
      cond_.Wait(&mutex_);
    }
    return {Exception::kSuccess};
  }
  void CountDown() override {
    absl::MutexLock lock(&mutex_);
    if (count_ > 0 && --count_ == 0) {
      cond_.SignalAll();
    }
  }

 private:
  absl::Mutex mutex_;   // Mutex to be used with cond_.Wait...() method family.
  absl::CondVar cond_;  // Condition to synchronize up to N waiting threads.
  int count_
      ABSL_GUARDED_BY(mutex_);  // When zero, latch should release all waiters.
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_IMPL_G3_COUNT_DOWN_LATCH_H_
