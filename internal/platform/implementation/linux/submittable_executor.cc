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

#include "internal/platform/implementation/linux/submittable_executor.h"

#include "internal/platform/implementation/linux/executor.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

SubmittableExecutor::SubmittableExecutor(size_t max_concurrancy)
    : executor_(std::make_unique<nearby::linux::Executor>(max_concurrancy)),
      shut_down_(false) {}

bool SubmittableExecutor::DoSubmit(Runnable&& wrapped_callable) {
  if (!shut_down_) {
    executor_->Execute(std::move(wrapped_callable));
    return true;
  }

  LOG(ERROR) << "Error: " << __func__
                     << ": Attempt to DoSubmit on a shutdown executor.";

  return false;
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
void SubmittableExecutor::Execute(Runnable&& runnable) {
  if (!shut_down_) {
    executor_->Execute(std::move(runnable));
  } else {
    LOG(ERROR) << "Error: " << __func__
                       << ": Attempt to Execute on a shutdown executor.";
  }
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
void SubmittableExecutor::Shutdown() {
  if (!shut_down_) {
    executor_->Shutdown();
    shut_down_ = true;
    return;
  }

  LOG(ERROR) << "Error: " << __func__
                     << ": Attempt to Shutdown on a shutdown executor.";
}

}  // namespace linux
}  // namespace nearby
