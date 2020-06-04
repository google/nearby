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

#ifndef PLATFORM_IMPL_SHARED_POSIX_LOCK_H_
#define PLATFORM_IMPL_SHARED_POSIX_LOCK_H_

#include <pthread.h>

#include "platform/api/lock.h"

namespace location {
namespace nearby {

class PosixLock : public Lock {
 public:
  PosixLock();
  ~PosixLock() override;

  void lock() override;
  void unlock() override;

 private:
  friend class PosixConditionVariable;

  pthread_mutexattr_t attr_;
  pthread_mutex_t mutex_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_POSIX_LOCK_H_
