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

#ifndef PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
#define PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_

#include <pthread.h>

#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/shared/posix_mutex.h"

namespace nearby {
namespace posix {

class ConditionVariable : public api::ConditionVariable {
 public:
  explicit ConditionVariable(Mutex* mutex);
  ~ConditionVariable() override;

  void Notify() override;
  Exception Wait() override;

 private:
  Mutex* mutex_;
  pthread_condattr_t attr_;
  pthread_cond_t cond_;
};

}  // namespace posix
}  // namespace nearby

#endif  // PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
