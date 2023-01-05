// Copyright 2022 Google LLC
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
#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_DIRECT_EXECUTOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_DIRECT_EXECUTOR_H_

#include "absl/base/thread_annotations.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/submittable_executor.h"

namespace nearby {

// An Executor that executes the tasks immediately on the thread calling
// `Execute()`.
//
// DirectExecutor is suitable for running fast, trivial runnables.
class ABSL_LOCKABLE DirectExecutor final : public api::Executor {
 public:
  DirectExecutor(const DirectExecutor &) = delete;
  DirectExecutor &operator=(const DirectExecutor &) = delete;
  static DirectExecutor &GetInstance() {
    // DirectExecutor isn't technically trivially-destructible because it has a
    // virtual destructor, but the class is final, the destructor is default
    // (the base class is an interface) and there are no member fields.
    // NOLINTNEXTLINE(google3-runtime-global-variables)
    static DirectExecutor instance;
    return instance;
  }

  void Execute(Runnable &&runnable) override { runnable(); }

  void Shutdown() override {}

 private:
  DirectExecutor() = default;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_DIRECT_EXECUTOR_H_
