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

#ifndef PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_API_SUBMITTABLE_EXECUTOR_H_

#include <memory>

#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/future.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace api {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(absl::AnyInvocable<void()>) method.
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override = default;

  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  virtual bool DoSubmit(Runnable&& wrapped_callable) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
