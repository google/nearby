// Copyright 2021 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_SYSTEM_CLOCK_H_
#define PLATFORM_IMPL_WINDOWS_SYSTEM_CLOCK_H_

#include "internal/platform/implementation/system_clock.h"

namespace nearby {

// Initialize global system state.
void SystemClock::Init() { }

// Returns current absolute time. It is guaranteed to be monotonic.
absl::Time SystemClock::ElapsedRealtime() {
  return absl::FromUnixNanos(
     std::chrono::duration_cast<std::chrono::nanoseconds>(
         std::chrono::steady_clock::now().time_since_epoch())
         .count());
}

// Pauses current thread for the specified duration.
Exception SystemClock::Sleep(absl::Duration duration) {
  absl::SleepFor(duration);
  return {Exception::kSuccess};
}

}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_SYSTEM_CLOCK_H_
