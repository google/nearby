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

#include "internal/platform/implementation/apple/count_down_latch.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace apple {

Exception CountDownLatch::Await() {
  absl::MutexLock lock(&mutex_, absl::Condition(IsZeroOrNegative, &count_));
  return {Exception::kSuccess};
}

ExceptionOr<bool> CountDownLatch::Await(absl::Duration timeout) {
  bool condition = mutex_.LockWhenWithTimeout(
      absl::Condition(IsZeroOrNegative, &count_), timeout);
  mutex_.Unlock();
  return ExceptionOr<bool>(condition);
}

void CountDownLatch::CountDown() {
  absl::MutexLock lock(&mutex_);
  count_--;
}

}  // namespace apple
}  // namespace nearby
