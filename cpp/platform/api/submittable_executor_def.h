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

#ifndef PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_
#define PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_

#include <functional>

#include "platform/api/executor.h"
#include "platform/api/future.h"
#include "platform/callable.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(std::function<void()>) method.
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override = default;

  template <typename T>
  Ptr<Future<T>> submit(Ptr<Callable<T>> callable);

 protected:
  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  virtual bool DoSubmit(std::function<void()> wrapped_callable) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SUBMITTABLE_EXECUTOR_DEF_H_
