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

#ifndef PLATFORM_V2_PUBLIC_FUTURE_H_
#define PLATFORM_V2_PUBLIC_FUTURE_H_

#include "platform_v2/api/executor.h"
#include "platform_v2/api/platform.h"
#include "platform_v2/api/settable_future.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/runnable.h"
#include "absl/time/time.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {

template <typename T>
class Future final : public api::SettableFuture<T> {
 public:
  using Platform = api::ImplementationPlatform;
  ~Future() override = default;
  Future() : impl_(Platform::CreateSettableFutureAny().release()) {}
  Future(Future&& other) = default;
  Future& operator=(Future&& other) = default;

  void AddListener(Runnable runnable, api::Executor* executor) override {
    impl_->AddListener(runnable, executor);
  }
  bool Set(const T& value) override { return impl_->Set(absl::any(value)); }
  bool Set(T&& value) override { return impl_->Set(absl::any(value)); }
  bool SetException(Exception exception) override {
    return impl_->SetException(exception);
  }
  // throws Exception::kInterrupted, Exception::kExecution
  ExceptionOr<T> Get() override {
    auto ret_val = impl_->Get();
    if (ret_val.ok()) {
      T result = std::any_cast<T>(ret_val.result());
      return ExceptionOr<T>{result};
    } else {
      return ExceptionOr<T>{ret_val.exception()};
    }
  }

  // throws Exception::kInterrupted, Exception::kExecution
  // throws Exception::kTimeout if timeout is exceeded while waiting for
  // result.
  ExceptionOr<T> Get(absl::Duration timeout) override {
    auto ret_val = impl_->Get(timeout);
    if (ret_val.ok()) {
      T result = std::any_cast<T>(ret_val.result());
      return ExceptionOr<T>{result};
    } else {
      return ExceptionOr<T>{ret_val.exception()};
    }
  }

 private:
  std::unique_ptr<api::SettableFuture<absl::any>> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_FUTURE_H_
