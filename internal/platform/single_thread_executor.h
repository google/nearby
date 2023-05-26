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

#ifndef PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_

#include "absl/base/thread_annotations.h"
#include "internal/platform/submittable_executor.h"

namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class ABSL_LOCKABLE SingleThreadExecutor : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor()
      : SubmittableExecutor(Platform::CreateSingleThreadExecutor()) {}
  ~SingleThreadExecutor() override = default;
  SingleThreadExecutor(SingleThreadExecutor&&) = default;
  SingleThreadExecutor& operator=(SingleThreadExecutor&&) = default;
};

// Moves the object to `executor` and destroys it there.
// This pattern is useful when there are some tasks running on `executor` that
// hold a reference to `object`. The tasks will complete before `object` is
// destroyed.
template <typename T>
void DestroyOnExecutor(T object, SingleThreadExecutor* executor) {
  executor->Execute([object = std::move(object)] {});
}

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
