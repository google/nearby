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

#ifndef PLATFORM_API_EXECUTOR_H_
#define PLATFORM_API_EXECUTOR_H_

#include "internal/platform/runnable.h"

namespace nearby {
namespace api {

int GetCurrentTid();

// This abstract class is the superclass of all classes representing an
// Executor.
class Executor {
 public:
  // Before returning from destructor, executor must wait for all pending
  // jobs to finish.
  virtual ~Executor() = default;
  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  virtual void Execute(Runnable&& runnable) = 0;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  virtual void Shutdown() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_EXECUTOR_H_
