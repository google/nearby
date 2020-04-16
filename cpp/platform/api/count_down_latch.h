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

#ifndef PLATFORM_API_COUNT_DOWN_LATCH_H_
#define PLATFORM_API_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "platform/exception.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch {
 public:
  virtual ~CountDownLatch() {}

  virtual Exception::Value await() = 0;  // throws Exception::INTERRUPTED
  virtual ExceptionOr<bool> await(
      std::int32_t timeout_millis) = 0;  // throws Exception::INTERRUPTED
  virtual void countDown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_COUNT_DOWN_LATCH_H_
