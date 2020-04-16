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

#include "platform/reliability_utils.h"

namespace location {
namespace nearby {

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         Ptr<Runnable> recovery_runnable) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         Ptr<Runnable> recovery_runnable,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         std::int64_t recovery_pause_millis) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         std::int64_t recovery_pause_millis,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         int num_attempts,
                                         std::int64_t recovery_pause_millis,
                                         Ptr<Runnable> recovery_runnable,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

}  // namespace nearby
}  // namespace location
