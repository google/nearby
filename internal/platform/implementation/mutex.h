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

#ifndef PLATFORM_API_MUTEX_H_
#define PLATFORM_API_MUTEX_H_

#include "absl/base/thread_annotations.h"

namespace nearby {
namespace api {

// A recursive lock for controlling access to a shared resource by multiple
// threads, allowing multiple acquisitions from the same thread.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class ABSL_LOCKABLE RecursiveMutex {
 public:
  virtual ~RecursiveMutex() = default;

  virtual void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() = 0;
  virtual void Unlock() ABSL_UNLOCK_FUNCTION() = 0;
  // Assert that this mutex is held by the calling thread.
  virtual void AssertHeld() const ABSL_ASSERT_EXCLUSIVE_LOCK() {}
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_MUTEX_H_
