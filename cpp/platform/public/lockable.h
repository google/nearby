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

#ifndef PLATFORM_PUBLIC_LOCKABLE_H_
#define PLATFORM_PUBLIC_LOCKABLE_H_

#include "absl/base/thread_annotations.h"

namespace nearby {

// A resource that can be locked. This class is provided
// for clang thread safety analysis at compile time.
// There is no actual locking at runtime.
class ABSL_LOCKABLE Lockable {
 private:
  void Acquire() const ABSL_EXCLUSIVE_LOCK_FUNCTION() {}
  void Release() const ABSL_UNLOCK_FUNCTION() {}
  friend class ThreadLockHolder;
};

// RAII holder for a Lockable resource.
class ABSL_SCOPED_LOCKABLE ThreadLockHolder {
 public:
  explicit ThreadLockHolder(const Lockable* lockable)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(*lockable)
      : lockable_{lockable} {
    lockable_->Acquire();
  }

  ~ThreadLockHolder() ABSL_UNLOCK_FUNCTION() { lockable_->Release(); }

 private:
  Lockable const* lockable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_LOCKABLE_H_
