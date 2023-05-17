// Copyright 2023 Google LLC
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

#ifndef PLATFORM_PUBLIC_FAKE_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_FAKE_SINGLE_THREAD_EXECUTOR_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {

class ABSL_LOCKABLE FakeSingleThreadExecutor final
    : public SingleThreadExecutor {
 public:
  FakeSingleThreadExecutor();
  ~FakeSingleThreadExecutor() override;
  FakeSingleThreadExecutor(FakeSingleThreadExecutor&&) = default;
  FakeSingleThreadExecutor& operator=(FakeSingleThreadExecutor&&) = default;

  void Execute(const std::string& name, Runnable&& runnable) override;

  void SetRunExecutablesImmediately(bool run_executables_immediately) {
    run_executables_immediately_ = run_executables_immediately;
  }

  void RunAllExecutables();

 private:
  void DoShutdown();

  bool run_executables_immediately_ = false;
  std::vector<std::pair<std::string, Runnable>> runnables_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_FAKE_SINGLE_THREAD_EXECUTOR_H_
