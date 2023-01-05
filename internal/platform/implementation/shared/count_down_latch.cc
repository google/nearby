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

#include "internal/platform/implementation/shared/count_down_latch.h"

namespace nearby {
namespace shared {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
CountDownLatch::CountDownLatch(int count) : count_(count) {}

ExceptionOr<bool> CountDownLatch::Await(absl::Duration timeout) {
  absl::MutexLock lock(&mutex_);
  absl::Time deadline = absl::Now() + timeout;
  while (count_ > 0) {
    if (cond_.WaitWithDeadline(&mutex_, deadline)) {
      return ExceptionOr<bool>(false);
    }
  }
  return ExceptionOr<bool>(true);
}

Exception CountDownLatch::Await() {
  absl::MutexLock lock(&mutex_);
  while (count_ > 0) {
    cond_.Wait(&mutex_);
  }
  return {Exception::kSuccess};
}
void CountDownLatch::CountDown() {
  absl::MutexLock lock(&mutex_);
  if (count_ > 0 && --count_ == 0) {
    cond_.SignalAll();
  }
}

}  // namespace shared
}  // namespace nearby
