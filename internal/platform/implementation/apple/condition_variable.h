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

#ifndef PLATFORM_IMPL_APPLE_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_APPLE_CONDITION_VARIABLE_H_

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/apple/mutex.h"

namespace nearby {
namespace apple {

// Concrete ConditionVariable implementation.
class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(apple::Mutex* mutex) : mutex_(&mutex->mutex_) {}
  ~ConditionVariable() override = default;

  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  Exception Wait() override;
  Exception Wait(absl::Duration timeout) override;
  void Notify() override;

 private:
  absl::Mutex* mutex_;
  absl::CondVar condition_variable_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_CONDITION_VARIABLE_H_
