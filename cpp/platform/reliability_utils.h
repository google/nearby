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

#ifndef PLATFORM_RELIABILITY_UTILS_H_
#define PLATFORM_RELIABILITY_UTILS_H_

#include <cstdint>

#include "platform/api/atomic_boolean.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

class ReliabilityUtils {
 public:
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                Ptr<Runnable> recovery_runnable);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                Ptr<Runnable> recovery_runnable,
                                const AtomicBoolean& isCancelled);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                std::int64_t recovery_pause_millis);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                std::int64_t recovery_pause_millis,
                                const AtomicBoolean& isCancelled);

 private:
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                int num_attempts,
                                std::int64_t recovery_pause_millis,
                                Ptr<Runnable> recovery_runnable,
                                const AtomicBoolean& isCancelled);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_RELIABILITY_UTILS_H_
