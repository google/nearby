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

#ifndef PLATFORM_IMPL_WINDOWS_COUNT_DOWN_LATCH_H_
#define PLATFORM_IMPL_WINDOWS_COUNT_DOWN_LATCH_H_

#include "platform/api/count_down_latch.h"

namespace location {
namespace nearby {
namespace windows {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch : public api::CountDownLatch {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~CountDownLatch() override = default;

  // TODO(b/184975123): replace with real implementation.
  Exception Await() override { return Exception{}; }
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<bool> Await(absl::Duration timeout) override {
    return Exception{};
  }
  // TODO(b/184975123): replace with real implementation.
  void CountDown() override{};
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_COUNT_DOWN_LATCH_H_
