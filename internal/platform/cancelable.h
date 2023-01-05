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

#ifndef PLATFORM_PUBLIC_CANCELABLE_H_
#define PLATFORM_PUBLIC_CANCELABLE_H_

#include <memory>
#include <utility>

#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/cancellable_task.h"

namespace nearby {

// An interface to provide a cancellation mechanism for objects that represent
// long-running operations.
class Cancelable final {
 public:
  Cancelable() = default;
  Cancelable(const Cancelable&) = default;
  Cancelable& operator=(const Cancelable& other) = default;

  ~Cancelable() = default;

  // This constructor is used internally only,
  // by other classes in "//platform/public/".
  explicit Cancelable(std::shared_ptr<CancellableTask> task,
                      std::shared_ptr<api::Cancelable> impl)
      : task_{task}, impl_(std::move(impl)) {}

  bool Cancel() {
    if (!impl_) return false;
    bool result = impl_->Cancel();
    task_->CancelAndWaitIfStarted();
    return result;
  }

  bool IsValid() { return impl_ != nullptr; }

 private:
  std::shared_ptr<CancellableTask> task_;
  std::shared_ptr<api::Cancelable> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_CANCELABLE_H_
