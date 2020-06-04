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

#ifndef PLATFORM_V2_API_LISTENABLE_FUTURE_H_
#define PLATFORM_V2_API_LISTENABLE_FUTURE_H_

#include <functional>
#include <memory>

#include "platform_v2/api/executor.h"
#include "platform_v2/api/future.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/runnable.h"

namespace location {
namespace nearby {
namespace api {

// A Future that accepts completion listeners.
//
// https://guava.dev/releases/20.0/api/docs/com/google/common/util/concurrent/ListenableFuture.html
template <typename T>
class ListenableFuture : public Future<T> {
 public:
  ~ListenableFuture() override = default;

  virtual void AddListener(Runnable runnable,
                           Executor* executor) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_LISTENABLE_FUTURE_H_
