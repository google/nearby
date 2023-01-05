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
#ifndef PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_
#include <windows.h>
#include <stdio.h>
#include <synchapi.h>

#include <mutex>  //  NOLINT

#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/windows/mutex.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace windows {

class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(api::Mutex* mutex)
      : mutex_(&(static_cast<windows::Mutex*>(mutex))->mutex_) {}
  ~ConditionVariable() override = default;

  Exception Wait() override {
    cond_var_.Wait(mutex_);
    return {Exception::kSuccess};
  }

  Exception Wait(absl::Duration timeout) override {
    cond_var_.WaitWithTimeout(mutex_, timeout);
    return {Exception::kSuccess};
  }

  void Notify() override { cond_var_.SignalAll(); }

 private:
  absl::Mutex* mutex_;
  absl::CondVar cond_var_;
};
}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_CONDITION_VARIABLE_H_
