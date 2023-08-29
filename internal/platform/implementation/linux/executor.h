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

#ifndef PLATFORM_IMPL_LINUX_EXECUTOR_H_
#define PLATFORM_IMPL_LINUX_EXECUTOR_H_

#include <atomic>
#include <memory>

#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/linux/thread_pool.h"

namespace nearby {
namespace linux {

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor : public api::Executor {
 public:
  Executor(size_t max_concurrency = 1);

  // Before returning from destructor, executor must wait for all pending
  // jobs to finish.
  ~Executor() override = default;

  void Execute(Runnable&& runnable) override;
  void Shutdown() override;

 private:
  std::unique_ptr<linux::ThreadPool> thread_pool_ = nullptr;
  std::atomic<bool> shut_down_ = false;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_EXECUTOR_H_
