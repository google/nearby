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

#ifndef PLATFORM_BASE_LISTENERS_H_
#define PLATFORM_BASE_LISTENERS_H_

#include "absl/functional/any_invocable.h"

namespace nearby {

// Provides default-initialization with a valid empty method,
// instead of nullptr. This allows partial initialization
// of a set of listeners.
template <typename... Args>
constexpr absl::AnyInvocable<void(Args...)> DefaultCallback() {
  return absl::AnyInvocable<void(Args...)>{[](Args...) {}};
}

template <typename... Args>
constexpr std::function<void(Args...)> DefaultFuncCallback() {
  return [](Args...) {};
}

}  // namespace nearby

#endif  // PLATFORM_BASE_LISTENERS_H_
