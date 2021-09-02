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

#include "platform/impl/windows/condition_variable.h"

namespace location {
namespace nearby {
namespace windows {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.

ConditionVariable::ConditionVariable(api::Mutex* mutex)
    : mutex_(dynamic_cast<windows::Mutex&>(*mutex)) {
  InitializeCriticalSection(&critical_section_);
}

// Notifies all the waiters that condition state has changed.
// TODO(b/184975123): replace with real implementation.
void ConditionVariable::Notify() {
  EnterCriticalSection(&critical_section_);

  condition_variable_actual_.notify_all();

  LeaveCriticalSection(&critical_section_);
}

// Waits indefinitely for Notify to be called.
// May return prematurely in case of interrupt, if supported by platform.
// Returns kSuccess, or kInterrupted on interrupt.
Exception ConditionVariable::Wait() {
  std::unique_lock<std::mutex> lock =
      std::unique_lock<std::mutex>(mutex_.GetWindowsMutex());
  condition_variable_actual_.wait(lock);

  return Exception{Exception::kSuccess};
}

// Waits while timeout has not expired for Notify to be called.
// May return prematurely in case of interrupt, if supported by platform.
// Returns kSuccess, or kInterrupted on interrupt.
Exception ConditionVariable::Wait(absl::Duration timeout) {
  auto now = std::chrono::system_clock::now();
  std::unique_lock<std::mutex> lock =
      std::unique_lock<std::mutex>(mutex_.GetWindowsMutex());
  if (condition_variable_actual_.wait_until(
          lock, now + absl::ToChronoMilliseconds(timeout)) ==
      std::cv_status::timeout) {
    return Exception{Exception::kInterrupted};
  } else {
    return Exception{Exception::kSuccess};
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
