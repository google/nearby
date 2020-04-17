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

#include <cstdint>

namespace location {
namespace nearby {

class SystemClock {
 public:
  virtual ~SystemClock() {}

  // Returns the time (in milliseconds) since the system was booted, and
  // includes deep sleep. This clock should be guaranteed to be monotonic, and
  // should continue to tick even when the CPU is in power saving modes.
  virtual std::int64_t elapsedRealtime() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SYSTEM_CLOCK_H_
