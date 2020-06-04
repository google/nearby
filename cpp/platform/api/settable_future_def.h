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

#ifndef PLATFORM_API_SETTABLE_FUTURE_DEF_H_
#define PLATFORM_API_SETTABLE_FUTURE_DEF_H_

#include "platform/api/listenable_future.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
//
// Platform must implentent non-template static member functions
//   Ptr<SettableFuture<size_t>> CreateSettableFutureSizeT()
//   Ptr<SettableFuture<std::shared_ptr<void>>> CreateSettableFuturePtr()
// in the location::nearby::platform::ImplementationPlatform class.
template <typename T>
class SettableFuture : public ListenableFuture<T> {
 public:
  ~SettableFuture() override = default;

  virtual bool set(T value) = 0;

  virtual bool setException(Exception exception) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SETTABLE_FUTURE_DEF_H_
