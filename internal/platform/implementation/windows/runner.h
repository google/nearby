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

#ifndef PLATFORM_IMPL_WINDOWS_RUNNER_H_
#define PLATFORM_IMPL_WINDOWS_RUNNER_H_

#include "internal/platform/runnable.h"

namespace location {
namespace nearby {
namespace windows {

class ThreadPool;

class Runner {
 public:
  Runner(std::function<void()> runnable)
      : thread_pool_(nullptr), runnable_(runnable) {}
  void Run() { runnable_(); }
  ~Runner(){}
  ThreadPool* thread_pool_;

 private:
  std::function<void()> runnable_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_RUNNER_H_
