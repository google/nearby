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

#include "internal/platform/implementation/shared/posix_condition_variable.h"

namespace nearby {
namespace posix {

ConditionVariable::ConditionVariable(Mutex* mutex)
    : mutex_(mutex), attr_(), cond_() {
  pthread_condattr_init(&attr_);

  pthread_cond_init(&cond_, &attr_);
}

ConditionVariable::~ConditionVariable() {
  pthread_cond_destroy(&cond_);

  pthread_condattr_destroy(&attr_);
}

void ConditionVariable::Notify() { pthread_cond_broadcast(&cond_); }

Exception ConditionVariable::Wait() {
  pthread_cond_wait(&cond_, &(mutex_->mutex_));

  return {Exception::kSuccess};
}

}  // namespace posix
}  // namespace nearby
