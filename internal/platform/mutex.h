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

#ifndef PLATFORM_PUBLIC_MUTEX_H_
#define PLATFORM_PUBLIC_MUTEX_H_

#include <memory>

#include "absl/base/thread_annotations.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {

#pragma push_macro("CreateMutex")
#undef CreateMutex

// This is a classic mutex can be acquired at most once.
// Atttempt to acuire mutex from the same thread that is holding it will likely
// cause a deadlock.
class ABSL_LOCKABLE Mutex final {
 public:
  explicit Mutex(bool check = true)
      : impl_(api::ImplementationPlatform::CreateMutex(
            check ? api::Mutex::Mode::kRegular
                  : api::Mutex::Mode::kRegularNoCheck)) {}
  Mutex(Mutex&&) = default;
  Mutex& operator=(Mutex&&) = default;
  ~Mutex() = default;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { impl_->Lock(); }
  void Unlock() ABSL_UNLOCK_FUNCTION() { impl_->Unlock(); }

 private:
  friend class ConditionVariable;
  friend class MutexLock;
  std::unique_ptr<api::Mutex> impl_;
};

// This mutex is compatible with Java definition:
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
// This mutex may be acuired multiple times by a thread that is already holding
// it without blocking.
// It needs to be released equal number of times before any other thread could
// successfully acquire it.
class ABSL_LOCKABLE RecursiveMutex final {
 public:
  RecursiveMutex()
      : impl_(api::ImplementationPlatform::CreateMutex(
            api::Mutex::Mode::kRecursive)) {}
  RecursiveMutex(RecursiveMutex&&) = default;
  RecursiveMutex& operator=(RecursiveMutex&&) = default;
  ~RecursiveMutex() = default;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() { impl_->Lock(); }
  void Unlock() ABSL_UNLOCK_FUNCTION() { impl_->Unlock(); }

 private:
  friend class MutexLock;
  std::unique_ptr<api::Mutex> impl_;
};

#pragma pop_macro("CreateMutex")

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_MUTEX_H_
