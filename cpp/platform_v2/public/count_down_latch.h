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

#ifndef PLATFORM_V2_PUBLIC_COUNT_DOWN_LATCH_H_
#define PLATFORM_V2_PUBLIC_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "platform_v2/api/count_down_latch.h"
#include "platform_v2/api/platform.h"
#include "platform_v2/base/exception.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit CountDownLatch(int count)
      : impl_(Platform::CreateCountDownLatch(count)) {}
  CountDownLatch(CountDownLatch&&) = default;
  CountDownLatch& operator=(CountDownLatch&&) = default;
  ~CountDownLatch() = default;

  Exception Await() { return impl_->Await(); }
  ExceptionOr<bool> Await(absl::Duration timeout) {
    return impl_->Await(timeout);
  }
  void CountDown() { impl_->CountDown(); }

 private:
  std::unique_ptr<api::CountDownLatch> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_COUNT_DOWN_LATCH_H_
