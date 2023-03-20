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

#ifndef PLATFORM_PUBLIC_FUTURE_H_
#define PLATFORM_PUBLIC_FUTURE_H_

#include <utility>

#include "internal/platform/settable_future.h"

namespace nearby {

template <typename T>
class Future final {
 public:
  using FutureCallback = typename SettableFuture<T>::FutureCallback;
  // Default Future. Does not time out.
  Future() : impl_(std::make_shared<SettableFuture<T>>()) {}

  // Creates a Future with a timeout.
  explicit Future(absl::Duration timeout)
      : impl_(std::make_shared<SettableFuture<T>>(timeout)) {}

  virtual bool Set(T value) { return impl_->Set(std::move(value)); }
  virtual bool SetException(Exception exception) {
    return impl_->SetException(exception);
  }
  virtual ExceptionOr<T> Get() { return impl_->Get(); }
  virtual ExceptionOr<T> Get(absl::Duration timeout) {
    return impl_->Get(timeout);
  }
  void AddListener(FutureCallback callback, api::Executor* executor) {
    impl_->AddListener(std::move(callback), executor);
  }
  bool IsSet() const { return impl_->IsSet(); }

 private:
  // Instance of future implementation is wrapped in shared_ptr<> to make
  // it possible to pass Future by value and share the implementation.
  // This allows for the following constructions:
  // 1)
  // Future<bool> future;
  // RunOnXyzThread([future]() { future.Set(DoTheJobAndReport()); });
  // if (future.Get().Ok()) { /*...*/ }
  // 2)
  // Future<bool> future = DoSomeAsyncWork(); // Returns future, but keeps copy.
  // if (future.Get().Ok()) { /*...*/ }
  std::shared_ptr<SettableFuture<T>> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_FUTURE_H_
