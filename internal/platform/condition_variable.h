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

#ifndef PLATFORM_PUBLIC_CONDITION_VARIABLE_H_
#define PLATFORM_PUBLIC_CONDITION_VARIABLE_H_

#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/exception.h"
#include "internal/platform/mutex.h"

namespace nearby {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit ConditionVariable(Mutex* mutex)
      : impl_(Platform::CreateConditionVariable(mutex->impl_.get())) {}
  ConditionVariable(ConditionVariable&&) = default;
  ConditionVariable& operator=(ConditionVariable&&) = default;

  void Notify() { impl_->Notify(); }
  Exception Wait() { return impl_->Wait(); }
  Exception Wait(absl::Duration timeout) { return impl_->Wait(timeout); }

 private:
  std::unique_ptr<api::ConditionVariable> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CONDITION_VARIABLE_H_
