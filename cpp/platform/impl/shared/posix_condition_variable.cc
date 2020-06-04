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

#include "platform/impl/shared/posix_condition_variable.h"

namespace location {
namespace nearby {

PosixConditionVariable::PosixConditionVariable(Ptr<PosixLock> lock)
    : lock_(lock), attr_(), cond_() {
  pthread_condattr_init(&attr_);

  pthread_cond_init(&cond_, &attr_);
}

PosixConditionVariable::~PosixConditionVariable() {
  pthread_cond_destroy(&cond_);

  pthread_condattr_destroy(&attr_);
}

void PosixConditionVariable::notify() { pthread_cond_broadcast(&cond_); }

Exception::Value PosixConditionVariable::wait() {
  pthread_cond_wait(&cond_, &(lock_->mutex_));

  return Exception::kSuccess;
}

}  // namespace nearby
}  // namespace location
