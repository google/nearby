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

#ifndef PLATFORM_IMPL_G3_MUTEX_H_
#define PLATFORM_IMPL_G3_MUTEX_H_

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/shared/posix_mutex.h"

namespace nearby {
namespace g3 {

class ABSL_LOCKABLE Mutex : public api::Mutex {
 public:
  explicit Mutex(bool check) : check_(check) {}
  ~Mutex() override = default;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(Mutex&&) = delete;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    mutex_.Lock();
    if (!check_) mutex_.ForgetDeadlockInfo();
  }
  void Unlock() ABSL_UNLOCK_FUNCTION() override { mutex_.Unlock(); }

 private:
  friend class ConditionVariable;
  absl::Mutex mutex_;
  bool check_;
};

class ABSL_LOCKABLE RecursiveMutex : public posix::Mutex {
 public:
  ~RecursiveMutex() override = default;
  RecursiveMutex() = default;
  RecursiveMutex(RecursiveMutex&&) = delete;
  RecursiveMutex& operator=(RecursiveMutex&&) = delete;
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_MUTEX_H_
