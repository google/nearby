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

#ifndef PLATFORM_IMPL_APPLE_MULTI_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_APPLE_MULTI_THREAD_EXECUTOR_H_

#import "internal/platform/implementation/apple/scheduled_executor.h"
#include "internal/platform/implementation/submittable_executor.h"
#import "internal/platform/runnable.h"

namespace nearby {
namespace apple {

class MultiThreadExecutor : public api::SubmittableExecutor {
 public:
  explicit MultiThreadExecutor(int max_concurrency);
  ~MultiThreadExecutor() override = default;

  MultiThreadExecutor(const MultiThreadExecutor&) = delete;
  MultiThreadExecutor& operator=(const MultiThreadExecutor&) = delete;

  // api::SubmittableExecutor:
  void Shutdown() override;
  void Execute(Runnable&& runnable) override;
  bool DoSubmit(Runnable&& runnable) override;

 private:
  std::unique_ptr<ScheduledExecutor> scheduled_executor_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_MULTI_THREAD_EXECUTOR_H_
