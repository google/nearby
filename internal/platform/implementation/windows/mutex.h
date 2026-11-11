// Copyright 2021 Google LLC
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
#ifndef PLATFORM_IMPL_WINDOWS_MUTEX_H_
#define PLATFORM_IMPL_WINDOWS_MUTEX_H_

#include <mutex>  //  NOLINT

#include "absl/base/thread_annotations.h"
#include "internal/platform/implementation/mutex.h"

namespace nearby {
namespace windows {

// A recursive lock for controlling access to a shared resource by multiple
// threads.
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class ABSL_LOCKABLE RecursiveMutex : public api::RecursiveMutex {
 public:
  RecursiveMutex() = default;
  ~RecursiveMutex() override = default;
  RecursiveMutex(RecursiveMutex&&) = delete;
  RecursiveMutex& operator=(RecursiveMutex&&) = delete;
  RecursiveMutex(const RecursiveMutex&) = delete;
  RecursiveMutex& operator=(const RecursiveMutex&) = delete;

  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION() override {
    recursive_mutex_.lock();
  }

  void Unlock() ABSL_UNLOCK_FUNCTION() override {
    recursive_mutex_.unlock();
  }

 private:
  std::recursive_mutex recursive_mutex_;  //  The actual mutex allocation
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_MUTEX_H_
