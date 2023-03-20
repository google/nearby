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

#ifndef PLATFORM_IMPL_WINDOWS_LISTENABLE_FUTURE_H_
#define PLATFORM_IMPL_WINDOWS_LISTENABLE_FUTURE_H_

#include "internal/platform/implementation/listenable_future.h"

namespace nearby {
namespace windows {

// A Future that accepts completion listeners.
//
// https://guava.dev/releases/20.0/api/docs/com/google/common/util/concurrent/ListenableFuture.html
template <typename T>
class ListenableFuture : public api::ListenableFuture<T> {
 public:
  using FutureCallback = typename api::ListenableFuture<T>::FutureCallback;
  // TODO(b/184975123): replace with real implementation.
  ~ListenableFuture() override = default;

  // TODO(b/184975123): replace with real implementation.
  void AddListener(FutureCallback callback, api::Executor* executor) {}
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_LISTENABLE_FUTURE_H_
