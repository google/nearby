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

#ifndef PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_

#include "absl/base/thread_annotations.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/submittable_executor.h"

namespace nearby {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newFixedThreadPool-int-
class ABSL_LOCKABLE MultiThreadExecutor final : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  explicit MultiThreadExecutor(int max_parallelism)
      : SubmittableExecutor(
            Platform::CreateMultiThreadExecutor(max_parallelism)) {}
  MultiThreadExecutor(MultiThreadExecutor&&) = default;
  MultiThreadExecutor& operator=(MultiThreadExecutor&&) = default;
  ~MultiThreadExecutor() override = default;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_MULTI_THREAD_EXECUTOR_H_
