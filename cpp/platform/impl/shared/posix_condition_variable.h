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

#include "platform/api/condition_variable.h"
#include "platform/impl/shared/posix_lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class PosixConditionVariable : public ConditionVariable {
 public:
  explicit PosixConditionVariable(Ptr<PosixLock> lock);
  ~PosixConditionVariable() override;

  void notify() override;
  Exception::Value wait() override;

 private:
  Ptr<PosixLock> lock_;
  pthread_condattr_t attr_;
  pthread_cond_t cond_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_CONDITION_VARIABLE_H_
