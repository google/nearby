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

#include "core/internal/loop_runner.h"

#include "platform/exception.h"

namespace location {
namespace nearby {
namespace connections {

LoopRunner::LoopRunner(const std::string& name) : name_(name) {}

bool LoopRunner::loop(Ptr<Callable<bool> > callable) {
  ScopedPtr<Ptr<Callable<bool> > > scoped_callable(callable);

  onEnterLoop();
  while (true) {
    onEnterIteration();
    ExceptionOr<bool> should_continue = scoped_callable->call();
    if (!should_continue.ok()) {
      onExceptionExitLoop(should_continue.exception());
      break;
    }

    onExitIteration();
    if (!should_continue.result()) {
      onExitLoop();
      return true;
    }
  }
  return false;
}

void LoopRunner::onEnterLoop() {
  // TODO(tracyzhou): Add logging.
}

void LoopRunner::onEnterIteration() {
  // TODO(tracyzhou): Add logging.
}

void LoopRunner::onExitIteration() {
  // TODO(tracyzhou): Add logging.
}

void LoopRunner::onExitLoop() {
  // TODO(tracyzhou): Add logging.
}

void LoopRunner::onExceptionExitLoop(Exception::Value exception) {
  // TODO(tracyzhou): Add logging.
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
