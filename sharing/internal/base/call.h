// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_CALL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_CALL_H_

#include <functional>
#include <utility>

namespace nearby {

// Nearby Share uses std::function to handle repeat callbacks and one time
// callbacks. In order to handle these callbacks in the correct way, use this
// template. If the right value of std::function is idempotent, that method can
// be moved. These methods also can be applied to any std::function.
template <typename R, typename... Args>
R Call(std::function<R(Args...)>&& func, Args... args) {
  return func(std::forward<Args>(args)...);
}

// This template function is used to call a function only once, as it  empties
// (moves) the original function after calling it.
// For a one time function, you should check if it is empty or not before
// calling it.
template <typename R, typename... Args>
R CallOnce(std::function<R(Args...)>&& func, Args... args) {
  auto call = std::move(func);
  return call(std::forward<Args>(args)...);
}

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_BASE_CALL_H_
