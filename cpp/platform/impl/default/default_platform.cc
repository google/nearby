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

#include "platform/impl/default/default_platform.h"

#include "platform/impl/default/default_condition_variable.h"
#include "platform/impl/default/default_lock.h"

namespace location {
namespace nearby {

Ptr<Lock> DefaultPlatform::createLock() { return MakePtr(new DefaultLock()); }

Ptr<ConditionVariable> DefaultPlatform::createConditionVariable(
    Ptr<Lock> lock) {
  return MakePtr(new DefaultConditionVariable(DowncastPtr<DefaultLock>(lock)));
}

}  // namespace nearby
}  // namespace location
