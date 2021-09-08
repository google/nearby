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

#ifndef PLATFORM_IMPL_WINDOWS_EXECUTOR_H_
#define PLATFORM_IMPL_WINDOWS_EXECUTOR_H_

#include <atomic>

#include "platform/api/executor.h"
#include "platform/impl/windows/thread_pool.h"

namespace location {
namespace nearby {
namespace windows {
enum class ExecutorState {
  Ready,    // has been created and initialized
  NotReady  // Executor has not been initialized
};

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor : public api::Executor {
 public:
  Executor();
  Executor(int32_t maxConcurrency);

  // Before returning from destructor, executor must wait for all pending
  // jobs to finish.
  ~Executor() override {}
  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  void Execute(Runnable&& runnable) override;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  void Shutdown() override;

 private:
  bool InitializeThreadPool();
  std::unique_ptr<ThreadPool> thread_pool_;

  std::atomic<bool> shut_down_;
  ExecutorState executor_state_;
  int32_t max_concurrency_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_EXECUTOR_H_
