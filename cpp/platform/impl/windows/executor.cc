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

#include "platform/impl/windows/executor.h"

#include "platform/impl/windows/generated/winrt/Windows.System.Threading.Core.h"
#include "platform/impl/windows/generated/winrt/Windows.System.Threading.h"
#include "platform/impl/windows/runner.h"
#include "platform/impl/windows/thread_pool.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace windows {

Executor::Executor() : Executor(1) {
  // Call thread pool creator
}

Executor::Executor(int32_t max_concurrency)
    : thread_pool_(std::make_unique<ThreadPool>(max_concurrency, false)),
      executor_state_(ExecutorState::NotReady),
      max_concurrency_(max_concurrency) {
  if (max_concurrency_ < 1) {
    throw(std::invalid_argument("max_concurrency"));
  }

  InitializeThreadPool();
}

bool Executor::InitializeThreadPool() {
  if (executor_state_ != ExecutorState::NotReady) {
    // To create a new pool, destroy the existing one first
    return false;
  }

  thread_pool_->SetPoolSize(max_concurrency_);
  thread_pool_->Create();
  executor_state_ = ExecutorState::Ready;

  return true;
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
void Executor::Execute(Runnable&& runnable) {
  if (shut_down_) {
    NEARBY_LOGS(VERBOSE) << "Warning: " << __func__
                       << ": Attempt to execute on a shut down pool.";
    return;
  }

  if (runnable == nullptr) {
    NEARBY_LOGS(VERBOSE) << "Error: " << __func__ << "Runnable was null.";
    return;
  }

  std::unique_ptr<Runner> runner = std::make_unique<Runner>(runnable);
  thread_pool_->Run(std::move(runner));
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
void Executor::Shutdown() {
  shut_down_ = true;
  thread_pool_ = nullptr;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
