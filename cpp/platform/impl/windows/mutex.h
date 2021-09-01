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

#include <windows.h>
#include <stdio.h>
#include <synchapi.h>

#include <memory>
#include <mutex>  //  NOLINT

#include "absl/memory/memory.h"
#include "platform/api/mutex.h"

namespace location {
namespace nearby {
namespace windows {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class Mutex : public api::Mutex {
 public:
  Mutex(Mutex::Mode mode);

  ~Mutex() override = default;

  void Lock() override;

  void Unlock() override;

  std::mutex& GetWindowsMutex();

  std::recursive_mutex& GetWindowsRecursiveMutex();

 private:
  Mutex::Mode mode_;
  bool locked_ = false;

  std::mutex mutex_actual_;
  std::mutex& mutex_ = mutex_actual_;
  std::recursive_mutex recursive_mutex_actual_;
  std::recursive_mutex& recursive_mutex_ = recursive_mutex_actual_;
  std::thread::id owning_thread_;
  CRITICAL_SECTION critical_section_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_MUTEX_H_
