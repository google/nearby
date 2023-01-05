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

#ifndef PLATFORM_IMPL_APPLE_MUTEX_H_
#define PLATFORM_IMPL_APPLE_MUTEX_H_

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/mutex.h"

namespace nearby {
namespace apple {

// Concrete Mutex implementation.
class ABSL_LOCKABLE Mutex : public api::Mutex {
 public:
  explicit Mutex() {}
  ~Mutex() override = default;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    mutex_.Lock();
    mutex_.ForgetDeadlockInfo();
  }
  void Unlock() ABSL_UNLOCK_FUNCTION() override { mutex_.Unlock(); }

 private:
  friend class ConditionVariable;
  absl::Mutex mutex_;
};

class ABSL_LOCKABLE RecursiveMutex : public api::Mutex {
 public:
  RecursiveMutex() = default;
  ~RecursiveMutex() override = default;

  RecursiveMutex(RecursiveMutex&&) = delete;
  RecursiveMutex& operator=(RecursiveMutex&&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    intptr_t thread_id = ThreadId();
    if (thread_id_.load(std::memory_order_acquire) != thread_id) {
      mutex_.Lock();
      thread_id_.store(thread_id, std::memory_order_release);
    }
    ++count_;
  }

  void Unlock() ABSL_UNLOCK_FUNCTION() override {
    if (--count_ == 0) {
      thread_id_.store(0, std::memory_order_release);
      mutex_.Unlock();
    }
  }

 private:
  static inline intptr_t ThreadId() {
    ABSL_CONST_INIT thread_local int per_thread = 0;
    return reinterpret_cast<intptr_t>(&per_thread);
  }

  std::atomic<intptr_t> thread_id_{0};
  int count_{0};
  absl::Mutex mutex_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_MUTEX_H_
