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

#ifndef PLATFORM_API_THREAD_UTILS_H_
#define PLATFORM_API_THREAD_UTILS_H_

#include <cstdint>

#include "platform/exception.h"

namespace location {
namespace nearby {

class ThreadUtils {
 public:
  virtual ~ThreadUtils() {}

  // https://docs.oracle.com/javase/7/docs/api/java/lang/Thread.html#sleep(long)
  virtual Exception::Value sleep(
      std::int64_t millis) = 0;  // throws Exception::INTERRUPTED
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_THREAD_UTILS_H_
