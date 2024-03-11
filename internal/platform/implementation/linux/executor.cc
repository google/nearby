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

#include "internal/platform/implementation/linux/executor.h"

#include <cassert>

#include "internal/platform/implementation/linux/thread_pool.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
Executor::Executor(size_t max_concurrency)
    : thread_pool_(std::make_unique<ThreadPool>(max_concurrency)) {
  assert(max_concurrency >= 1);
  assert(thread_pool_ != nullptr);
}

void Executor::Execute(Runnable &&runnable) {
  if (shut_down_) {
    NEARBY_LOGS(VERBOSE) << "Warning: " << __func__
                         << ": Attempt to execute on a shut down pool.";
    return;
  }

  if (runnable == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": Runnable was null.";
    return;
  }

  thread_pool_->Run(std::move(runnable));
}

void Executor::Shutdown() {
  shut_down_ = true;
  thread_pool_->ShutDown();
  thread_pool_ = nullptr;
}

}  // namespace linux
}  // namespace nearby
