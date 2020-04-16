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

#ifndef CORE_INTERNAL_LOOP_RUNNER_H_
#define CORE_INTERNAL_LOOP_RUNNER_H_

#include "platform/callable.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

// Construct to run a loop repeatedly. This class is useful to increase
// testability for multi-threaded code that runs loops; it shouldn't be used for
// general purpose loops unless tests require fine-grained control over the
// looping procedure.
class LoopRunner {
 public:
  explicit LoopRunner(const std::string& name);

  // Runs the provided callable repeatedly until it returns false.
  //
  // @return true if the loop completed successfully, false if an exception was
  // encountered.
  bool loop(Ptr<Callable<bool> > callable);

 protected:
  void onEnterLoop();
  void onEnterIteration();
  void onExitIteration();
  void onExitLoop();
  void onExceptionExitLoop(Exception::Value exception);

 private:
  const std::string name_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_LOOP_RUNNER_H_
