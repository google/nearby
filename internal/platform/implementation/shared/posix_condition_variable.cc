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
#include "absl/time/time.h"
#include <cerrno>

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

Exception ConditionVariable::Wait(absl::Duration timeout) {
  // Calculate absolute deadline as timespec for pthread_cond_timedwait.
  absl::Time deadline = absl::Now() + timeout;
  int64_t nanos = absl::ToUnixNanos(deadline);
  timespec ts{};  // zero-initialize to satisfy static analyzers
  ts.tv_sec = static_cast<time_t>(nanos / 1000000000LL);
  ts.tv_nsec = static_cast<long>(nanos % 1000000000LL);

  int rc = pthread_cond_timedwait(&cond_, &(mutex_->mutex_), &ts);
  if (rc == 0) return {Exception::kSuccess};
  if (rc == ETIMEDOUT) return {Exception::kTimeout};
  if (rc == EINTR) return {Exception::kInterrupted};
  return {Exception::kFailed};
}

}  // namespace posix
}  // namespace nearby
