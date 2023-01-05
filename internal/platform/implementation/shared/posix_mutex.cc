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

#include "internal/platform/implementation/shared/posix_mutex.h"

namespace nearby {
namespace posix {

Mutex::Mutex() : attr_(), mutex_() {
  pthread_mutexattr_init(&attr_);
  pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&mutex_, &attr_);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(&mutex_);

  pthread_mutexattr_destroy(&attr_);
}

void Mutex::Lock() { pthread_mutex_lock(&mutex_); }

void Mutex::Unlock() { pthread_mutex_unlock(&mutex_); }

}  // namespace posix
}  // namespace nearby
