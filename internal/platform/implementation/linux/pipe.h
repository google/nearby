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

#ifndef PLATFORM_IMPL_LINUX_PIPE_H_
#define PLATFORM_IMPL_LINUX_PIPE_H_

#include <memory>

#include "internal/platform/base_pipe.h"
#include "internal/platform/implementation/linux/condition_variable.h"
#include "internal/platform/implementation/linux/mutex.h"

namespace location {
namespace nearby {
namespace linux {

class Pipe : public BasePipe {
 public:
  Pipe() {
    auto mutex = std::make_unique<linux::Mutex>(/*check=*/true);
    auto cond = std::make_unique<linux::ConditionVariable>(mutex.get());
    Setup(std::move(mutex), std::move(cond));
  }
  ~Pipe() override = default;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;
};

}  // namespace linux
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_LINUX_PIPE_H_
