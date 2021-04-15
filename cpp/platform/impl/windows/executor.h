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

#include "platform/api/executor.h"

namespace location {
namespace nearby {
namespace windows {

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor : public api::Executor {
 public:
  // Before returning from destructor, executor must wait for all pending
  // jobs to finish.
  // TODO(b/184975123): replace with real implementation.
  ~Executor() override = default;
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

#endif  // PLATFORM_IMPL_WINDOWS_EXECUTOR_H_
