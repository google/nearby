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

#ifndef PLATFORM_IMPL_WINDOWS_SETTABLE_FUTURE_H_
#define PLATFORM_IMPL_WINDOWS_SETTABLE_FUTURE_H_

#include "internal/platform/implementation/settable_future.h"

namespace nearby {
namespace windows {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
template <typename T>
class SettableFuture : public api::SettableFuture<T> {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~SettableFuture() override = default;

  // Completes the future successfully. The value is returned to any waiters.
  // Returns true, if value was set.
  // Returns false, if Future is already in "done" state.
  // TODO(b/184975123): replace with real implementation.
  bool Set(T value) override { return false; }

  // Completes the future unsuccessfully. The exception value is returned to any
  // waiters.
  // Returns true, if exception was set.
  // Returns false, if Future is already in "done" state.
  // TODO(b/184975123): replace with real implementation.
  bool SetException(Exception exception) override { return false; }
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_SETTABLE_FUTURE_H_
