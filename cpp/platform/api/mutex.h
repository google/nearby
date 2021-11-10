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

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class ABSL_LOCKABLE Mutex {
 public:
  // Mode to pass to implementation constructor.
  // kRegular        - produces a regular mutex: disallows multiple locks from
  //                   the same thread; optionally, detects double locks in
  //                   debug mode.
  //                   This is the default option.
  // kRecursive      - produces recursive mutex: allows multiple locks from the
  //                   same thread.
  // kRegularNoCheck - produces a regular mutex: disallows double locks,
  //                   but does not check for deadlocks.
  enum class Mode {
    kRegular = 0,
    kRecursive = 1,
    kRegularNoCheck = 2,
  };

  virtual ~Mutex() {}

  virtual void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() = 0;
  virtual void Unlock() ABSL_UNLOCK_FUNCTION() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_MUTEX_H_
