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

#import "internal/platform/implementation/apple/multi_thread_executor.h"

#import "internal/platform/implementation/apple/scheduled_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace apple {

MultiThreadExecutor::MultiThreadExecutor(int max_concurrency) {
  scheduled_executor_ = std::make_unique<ScheduledExecutor>(max_concurrency);
}

void MultiThreadExecutor::Shutdown() { scheduled_executor_->Shutdown(); }

void MultiThreadExecutor::Execute(Runnable&& runnable) {
  scheduled_executor_->Execute(std::move(runnable));
}

bool MultiThreadExecutor::DoSubmit(Runnable&& runnable) {
  return scheduled_executor_->DoSubmit(std::move(runnable));
}

}  // namespace apple
}  // namespace nearby
