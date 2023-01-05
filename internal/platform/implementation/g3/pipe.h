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

#ifndef PLATFORM_IMPL_G3_PIPE_H_
#define PLATFORM_IMPL_G3_PIPE_H_

#include <memory>

#include "internal/platform/base_pipe.h"
#include "internal/platform/implementation/g3/condition_variable.h"
#include "internal/platform/implementation/g3/mutex.h"

namespace nearby {
namespace g3 {

class Pipe : public BasePipe {
 public:
  Pipe() {
    auto mutex = std::make_unique<g3::Mutex>(/*check=*/true);
    auto cond = std::make_unique<g3::ConditionVariable>(mutex.get());
    Setup(std::move(mutex), std::move(cond));
  }
  ~Pipe() override = default;
  Pipe(Pipe&&) = delete;
  Pipe& operator=(Pipe&&) = delete;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_PIPE_H_
