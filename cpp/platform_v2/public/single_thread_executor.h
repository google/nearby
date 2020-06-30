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

#ifndef PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_

#include "platform_v2/public/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class SingleThreadExecutor final : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor()
      : SubmittableExecutor(Platform::CreateSingleThreadExecutor()) {}
  ~SingleThreadExecutor() override = default;
  SingleThreadExecutor(SingleThreadExecutor&&) = default;
  SingleThreadExecutor& operator=(SingleThreadExecutor&&) = default;
  int Tid() const { return GetTid(0); }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
