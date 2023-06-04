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

#include "internal/test/fake_multi_thread_executor.h"

#include <string>
#include <utility>
#include <vector>

#include "internal/platform/multi_thread_executor.h"

namespace nearby {

FakeMultiThreadExecutor::FakeMultiThreadExecutor(int max_parallelism)
    : MultiThreadExecutor(max_parallelism) {}

FakeMultiThreadExecutor::~FakeMultiThreadExecutor() { DoShutdown(); }

void FakeMultiThreadExecutor::Execute(const std::string& name,
                                      Runnable&& runnable) {
  runnables_.push_back(std::make_pair(name, std::move(runnable)));

  if (!run_executables_immediately_) return;

  RunAllExecutables();
}

void FakeMultiThreadExecutor::RunAllExecutables() {
  // Because `MultiThreadExecutor` ensures sequencing, run all pending
  // executables in order they were added to the vector.
  for (auto& runnable_pair : runnables_) {
    runnable_pair.second();
  }

  runnables_.clear();
}

void FakeMultiThreadExecutor::DoShutdown() { RunAllExecutables(); }

}  // namespace nearby
