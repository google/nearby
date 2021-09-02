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

#include "platform/impl/windows/mutex.h"

namespace location {
namespace nearby {
namespace windows {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
Mutex::Mutex(Mutex::Mode mode) : mode_(mode) {}

void Mutex::Lock() {
  if (mode_ == Mutex::Mode::kRegular || mode_ == Mutex::Mode::kRegularNoCheck) {
    mutex_impl_.lock();
  } else if (mode_ == Mutex::Mode::kRecursive) {
    recursive_mutex_impl_.lock();
  }
}

void Mutex::Unlock() {
  if (mode_ == Mutex::Mode::kRegular || mode_ == Mutex::Mode::kRegularNoCheck) {
    mutex_impl_.unlock();
  } else if (mode_ == Mutex::Mode::kRecursive) {
    recursive_mutex_impl_.unlock();
  }
}

std::mutex& Mutex::GetWindowsMutex() { return mutex_; }

std::recursive_mutex& Mutex::GetWindowsRecursiveMutex() {
  return recursive_mutex_;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
