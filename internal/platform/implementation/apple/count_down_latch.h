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

#ifndef PLATFORM_IMPL_APPLE_COUNT_DOWN_LATCH_H_
#define PLATFORM_IMPL_APPLE_COUNT_DOWN_LATCH_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/count_down_latch.h"

namespace nearby {
namespace apple {

// Concrete CountDownLatch implementation.
class CountDownLatch : public api::CountDownLatch {
 public:
  explicit CountDownLatch(int count) : count_(count) {}
  ~CountDownLatch() override = default;

  CountDownLatch(const CountDownLatch&) = delete;
  CountDownLatch& operator=(const CountDownLatch&) = delete;

  Exception Await() override;
  ExceptionOr<bool> Await(absl::Duration timeout) override;
  void CountDown() override;

 private:
  static bool IsZeroOrNegative(int* count) { return 0 >= *count; }

  absl::Mutex mutex_;
  int count_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_COUNT_DOWN_LATCH_H_
