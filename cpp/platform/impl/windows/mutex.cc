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
Mutex::Mutex(Mutex::Mode mode)
    : mode_(mode), owning_thread_(std::this_thread::get_id()) {
  InitializeCriticalSection(&critical_section_);
}

void Mutex::Lock() {
  EnterCriticalSection(&critical_section_);

  std::thread::id currentThread = std::this_thread::get_id();

  if ((mode_ == Mutex::Mode::kRegular ||
       mode_ == Mutex::Mode::kRegularNoCheck) &&
      !locked_) {
    mutex_actual_.lock();
    owning_thread_ = currentThread;
    locked_ = true;
  }

  if (mode_ == Mutex::Mode::kRecursive) {
    if (!locked_) {
      owning_thread_ = currentThread;
    }
    if (owning_thread_ == currentThread) {
      try {
        recursive_mutex_actual_.lock();
      } catch ([[maybe_unused]] const std::system_error& e) {
      // Eat the exception and fail silently, argument left for debug
      }
      locked_ = true;
    }
  }

  LeaveCriticalSection(&critical_section_);
}

void Mutex::Unlock() {
  EnterCriticalSection(&critical_section_);

  std::thread::id currentThread = std::this_thread::get_id();

  if (mode_ == Mutex::Mode::kRegular || mode_ == Mutex::Mode::kRegularNoCheck) {
    if (currentThread == owning_thread_) {
      mutex_actual_.unlock();
      locked_ = false;
    }
  }

  if (mode_ == Mutex::Mode::kRecursive) {
    if (currentThread == owning_thread_) {
      recursive_mutex_actual_.unlock();
      locked_ = false;
    }
  }

  LeaveCriticalSection(&critical_section_);
}

std::mutex& Mutex::GetWindowsMutex() { return mutex_; }

std::recursive_mutex& Mutex::GetWindowsRecursiveMutex() {
  return recursive_mutex_;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
