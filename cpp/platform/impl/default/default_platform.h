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

#ifndef PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_
#define PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_

#include "platform/api/condition_variable.h"
#include "platform/api/lock.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Provides obvious portable implementations of a subset of the hooks specified
// within //platform/api/.
//
// It's highly recommended that custom Platform implementations delegate to
// these methods unless there's a very good reason not to.
class DefaultPlatform {
 public:
  static Ptr<Lock> createLock();

  static Ptr<ConditionVariable> createConditionVariable(Ptr<Lock> lock);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_DEFAULT_DEFAULT_PLATFORM_H_
