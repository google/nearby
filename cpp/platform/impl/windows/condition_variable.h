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

#ifndef PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_

#include <windows.h>
#include <stdio.h>
#include <synchapi.h>

#include <mutex>  //  NOLINT

#include "platform/api/condition_variable.h"
#include "platform/impl/windows/mutex.h"

namespace location {
namespace nearby {
namespace windows {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable : public api::ConditionVariable {
 public:
  ConditionVariable(api::Mutex* mutex);

  ~ConditionVariable() override = default;

  // Notifies all the waiters that condition state has changed.
  void Notify() override;

  // Waits indefinitely for Notify to be called.
  // May return prematurely in case of interrupt, if supported by platform.
  // Returns kSuccess, or kInterrupted on interrupt.
  Exception Wait() override;

  // Waits while timeout has not expired for Notify to be called.
  // May return prematurely in case of interrupt, if supported by platform.
  // Returns kSuccess, or kInterrupted on interrupt.
  Exception Wait(absl::Duration timeout) override;

 private:
  location::nearby::windows::Mutex& mutex_;

  std::condition_variable condition_variable_actual_;
  std::condition_variable& condition_variable_ = condition_variable_actual_;

  std::unique_lock<std::mutex> lock;
  CRITICAL_SECTION critical_section_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_
