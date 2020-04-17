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

#ifndef PLATFORM_API_CONDITION_VARIABLE_H_
#define PLATFORM_API_CONDITION_VARIABLE_H_

#include "platform/exception.h"

namespace location {
namespace nearby {

// The ConditionVariable class is a synchronization primitive that can be used
// to block a thread, or multiple threads at the same time, until another thread
// both modifies a shared variable (the condition), and notifies the
// ConditionVariable.
class ConditionVariable {
 public:
  virtual ~ConditionVariable() {}

  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#notify--
  virtual void notify() = 0;
  // https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#wait--
  virtual Exception::Value wait() = 0;  // throws Exception::INTERRUPTED
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_CONDITION_VARIABLE_H_
