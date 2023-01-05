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

#ifndef PLATFORM_API_SYSTEM_CLOCK_H_
#define PLATFORM_API_SYSTEM_CLOCK_H_

#include "absl/time/clock.h"
#include "internal/platform/exception.h"

namespace nearby {

class SystemClock final {
 public:
  // Initialize global system state.
  static void Init();
  // Returns current absolute time. It is guaranteed to be monotonic.
  static absl::Time ElapsedRealtime();
  // Pauses current thread for the specified duration.
  static Exception Sleep(absl::Duration duration);
};

}  // namespace nearby

#endif  // PLATFORM_API_SYSTEM_CLOCK_H_
