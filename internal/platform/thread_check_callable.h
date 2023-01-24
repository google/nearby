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

#ifndef PLATFORM_PUBLIC_THREAD_CHECK_CALLABLE_H_
#define PLATFORM_PUBLIC_THREAD_CHECK_CALLABLE_H_

#include "absl/base/thread_annotations.h"
#include "internal/platform/callable.h"
#include "internal/platform/lockable.h"

namespace nearby {

// A callable that acquires a lockable resource while running.
// This class helps with thread safety analysis.
template <typename T>
class ThreadCheckCallable {
 public:
  ThreadCheckCallable(const Lockable *lockable, Callable<T> callable)
      : lockable_(lockable), callable_(std::move(callable)) {}

  ExceptionOr<T> operator()() {
    ThreadLockHolder thread_lock(lockable_);
    return callable_();
  }

 private:
  Lockable const *lockable_;
  Callable<T> callable_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_THREAD_CHECK_CALLABLE_H_
