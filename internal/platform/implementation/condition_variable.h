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

#ifndef PLATFORM_API_CONDITION_VARIABLE_H_
#define PLATFORM_API_CONDITION_VARIABLE_H_

#include "absl/time/clock.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace api {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable {
 public:
  virtual ~ConditionVariable() {}

  // Notifies all the waiters that condition state has changed.
  virtual void Notify() = 0;

  // Waits indefinitely for Notify to be called.
  // May return prematurely in case of interrupt, if supported by platform.
  // Returns kSuccess, or kInterrupted on interrupt.
  virtual Exception Wait() = 0;

  // Waits while timeout has not expired for Notify to be called.
  // May return prematurely in case of interrupt, if supported by platform.
  // Returns kSuccess, or kInterrupted on interrupt.
  virtual Exception Wait(absl::Duration timeout) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_CONDITION_VARIABLE_H_
