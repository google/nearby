// ...existing code...
// filepath: /workspace/internal/platform/implementation/linux/system_clock.cc
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

#include "internal/platform/implementation/system_clock.h"

#include <chrono>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"

namespace nearby {

// Initialize global system state.
void SystemClock::Init() {}

// Returns current elapsed (monotonic) time.
absl::Time SystemClock::ElapsedRealtime() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

  // Represent monotonic time as an absl::Time value.
  // (Anchor is arbitrary; only differences matter for elapsed time.)
  return absl::FromUnixNanos(static_cast<int64_t>(nanos));
}

// Pauses current thread for the specified duration.
Exception SystemClock::Sleep(absl::Duration duration) {
  absl::SleepFor(duration);
  return {Exception::kSuccess};
}

}  // namespace nearby


