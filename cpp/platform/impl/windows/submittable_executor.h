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

#ifndef PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_

#include "platform/api/submittable_executor.h"

namespace location {
namespace nearby {
namespace windows {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(std::function<void()>) method.
class SubmittableExecutor : public api::SubmittableExecutor {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~SubmittableExecutor() override = default;

  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  // TODO(b/184975123): replace with real implementation.
  bool DoSubmit(Runnable&& wrapped_callable) override { return false; }

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  // TODO(b/184975123): replace with real implementation.
  void Execute(Runnable&& runnable) override {}

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  // TODO(b/184975123): replace with real implementation.
  void Shutdown() override {}

  // TODO(b/184975123): replace with real implementation.
  int GetTid(int index) const override { return 0; }
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_
