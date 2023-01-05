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

#ifndef PLATFORM_IMPL_WINDOWS_FUTURE_H_
#define PLATFORM_IMPL_WINDOWS_FUTURE_H_

#include "internal/platform/implementation/future.h"

namespace nearby {
namespace windows {

// A Future represents the result of an asynchronous computation.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Future.html
template <typename T>
class Future : public api::Future<T> {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~Future() override = default;

  // throws Exception::kInterrupted, Exception::kExecution
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<T> Get() override { return ExceptionOr<T>{Exception::kFailed}; }

  // throws Exception::kInterrupted, Exception::kExecution
  // throws Exception::kTimeout if timeout is exceeded while waiting for
  // result.
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<T> Get(absl::Duration timeout) override {
    return ExceptionOr<T>{Exception::kFailed};
  }
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_FUTURE_H_
