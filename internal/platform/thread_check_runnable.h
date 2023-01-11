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

#ifndef PLATFORM_PUBLIC_THREAD_CHECK_RUNNABLE_H_
#define PLATFORM_PUBLIC_THREAD_CHECK_RUNNABLE_H_

#include <utility>

#include "internal/platform/lockable.h"
#include "internal/platform/runnable.h"

namespace nearby {

// A runnable that acquires a lockable resource while running.
// This class helps with thread safety analysis.
class ThreadCheckRunnable {
 public:
  ThreadCheckRunnable(const Lockable *lockable, Runnable &&runnable)
      : lockable_{lockable}, runnable_{std::move(runnable)} {}

  void operator()() {
    ThreadLockHolder thread_lock(lockable_);
    runnable_();
  }

 private:
  Lockable const *lockable_;
  Runnable runnable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_THREAD_CHECK_RUNNABLE_H_
